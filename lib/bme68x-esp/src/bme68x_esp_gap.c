/*
 * Copyright (c) 2025, Christophe Dufaza
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Implementation note:
 *
 * Events causing state transitions are received from the Bluetooth stack through
 * callbacks running on cooperative threads:
 * - disconnected(), recycled(): run on the sysworkq thread (e.g. with priority -1)
 * - connected(): runs on the BT RX WQ thread (e.g. with priority -8),
 *   and can preempt the above callbacks
 *
 * To avoid concurrent state transitions and inversion of priority issues,
 * we'll sequence the events from the Bluetooth stack to a dedicated message queue.
 * Some lock is still needed to also allow state transitions to happen outside of
 * the message queue:
 * - synchronous API calls like bme68x_esp_gap_adv_start() will immediately return -EBUSY
 *   if the lock is already taken
 * - the message queue is consumed by a high priority preemptible thread that can actually
 *   move to the Waiting state until the lock is free
 *
 * This might seem like a bit of an overkill, just to implement a state machine that automatically
 * restarts advertising when expected.
 * This is a known issue, which the Zephyr project is working on, and a refactoring is in progress
 * to post all Bluetooth connection events to the same work queue.
 * We will reconsider our approach when this work has matured.
 */

#include "bme68x_esp_gap.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Should we automatically resume advertising. */
#ifdef CONFIG_BME68X_ESP_GAP_ADV_AUTO
#define CONN_MGR_CFG_FLAGS BME68X_GAP_CFG_ADV_AUTO
#else
#define CONN_MGR_CFG_FLAGS 0UL
#endif

#define CONN_MGR_MAX_CONN   CONFIG_BT_MAX_CONN
#define CONN_MGR_STACK_SIZE 1024
#define CONN_MGR_MAX_MSG    (2 * CONFIG_BT_MAX_CONN)
#define CONN_MGR_MSGQ_NAME  "ESP Conn Mgr"
/* Message queue consumer: highest priority for preemptible threads,
 * allows other preemptible threads to yield and retry when acquiring the lock.
 */
#define CONN_MGR_PRIORITY   0

#define LE_ADDR_STR(A, S)                                                                          \
	char S[BT_ADDR_LE_STR_LEN];                                                                \
	bt_addr_le_to_str((A), S, sizeof(S))

enum conn_mgr_event_id {
	CONN_MGR_EVT_CONNECTION,
	CONN_MGR_EVT_DISCONNECTION,
	CONN_MGR_EVT_CONN_RECYCLED,
};
struct conn_mgr_event_connection {
	/* Can be const-qualified, bt_conn_ref() happens outside of our message queue. */
	struct bt_conn const *conn;
	uint8_t const err;
};
struct conn_mgr_event_disconnection {
	/* Cannot be const-qualified, bt_conn_unref() happens in our message queue. */
	struct bt_conn *conn;
	uint8_t const reason;
};

struct conn_mgr_msg {
	enum conn_mgr_event_id const event;
	union {
		struct conn_mgr_event_connection const connection;
		struct conn_mgr_event_disconnection const disconnection;
	};
};

LOG_MODULE_REGISTER(esp_gap, CONFIG_BME68X_ESP_LOG_LEVEL);

/* Connection management messages queue. */
K_MSGQ_DEFINE(conn_mgr_msgq, sizeof(struct conn_mgr_msg), CONN_MGR_MAX_MSG, 1);
/* Consumer thread's stack. */
K_THREAD_STACK_DEFINE(conn_mgr_stack_area, CONN_MGR_STACK_SIZE);
/* Consumer thread's entry point. */
static void conn_mgr_msgq_recv(void *p1, void *p2, void *p3);

/* Connection management callbacks. */
static void cb_conn_connected(struct bt_conn *conn, uint8_t err);
static void cb_conn_disconnected(struct bt_conn *conn, uint8_t reason);
static void cb_conn_recycled(void);
#ifdef CONFIG_BT_SMP
static void cb_conn_security_changed(struct bt_conn *conn, bt_security_t level,
				     enum bt_security_err err);
#endif /* CONFIG_BT_SMP */

static struct bt_conn_cb conn_mgr_conn_callbacks = {
	/* Runs on BT RX WQ thread (default priority -8). */
	.connected = cb_conn_connected,
	/* Runs on sysworkq thread (default priority -1). */
	.disconnected = cb_conn_disconnected,
	/* Runs on sysworkq thread. */
	.recycled = cb_conn_recycled,
#ifdef CONFIG_BT_SMP
	/* Runs on BT RX WQ thread. */
	.security_changed = cb_conn_security_changed,
#endif /* CONN_MGR_SMP_ENABLED */
};

/* Connection manager state. */
static struct conn_mgr {
	/* Advertising data. */
	struct bt_data const *adv_data;
	size_t adv_data_len;
	/* Scan response data. */
	struct bt_data const *scan_data;
	size_t scan_data_len;

	/* Configuration and state flags. */
	uint32_t flags;
	/* Available connection resources (decremented on new connections,
	 * incremented when connections are recycled). */
	uint8_t conn_avail;

	/* Should be acquired (resp. released) when initiating (resp. completing)
	 * a state transition.
	 * We post all connections management events from various work queues
	 * to a dedicated message queue where we will handle them sequentially
	 * and possibly wait for the lock, which we can't do on the BT RX WQ
	 * or sysworkq threads.
	 */
	struct k_sem state_lock;

	/* Consumer thread for connection management messages. */
	struct k_thread tcb_msgq;

	/* Optional callback to notify state transitions. */
	bme68x_gap_state_changed_cb cb_gap_state_changed;

} conn_mgr;

static void conn_manager_init(struct bme68x_esp_gap_adv_cfg const *adv_cfg, uint32_t flags,
			      bme68x_gap_state_changed_cb cb_gap_state_changed)
{
	conn_mgr.adv_data = adv_cfg->adv_data;
	conn_mgr.adv_data_len = adv_cfg->adv_data_len;
	conn_mgr.scan_data = adv_cfg->scan_data;
	conn_mgr.scan_data_len = adv_cfg->scan_data_len;

	conn_mgr.cb_gap_state_changed = cb_gap_state_changed;

	conn_mgr.flags = flags;
	conn_mgr.conn_avail = CONN_MGR_MAX_CONN;

	k_sem_init(&conn_mgr.state_lock, 1, 1);

	k_tid_t tid = k_thread_create(&conn_mgr.tcb_msgq, conn_mgr_stack_area, CONN_MGR_STACK_SIZE,
				      conn_mgr_msgq_recv, NULL, NULL, NULL, CONN_MGR_PRIORITY, 0,
				      K_NO_WAIT);
	k_thread_name_set(tid, CONN_MGR_MSGQ_NAME);
}

static inline bool conn_mgr_acquire_state(uint32_t *flags, uint8_t *conn_avail, k_timeout_t timeout)
{
	if (k_sem_take(&conn_mgr.state_lock, timeout) != 0) {
		return false;
	}

	*flags = conn_mgr.flags;
	*conn_avail = conn_mgr.conn_avail;
	return true;
}

static inline void conn_mgr_release_state(uint32_t flags, uint8_t conn_avail)
{
	LOG_DBG("0x%08x (%hu/%hu)", flags, conn_avail, CONN_MGR_MAX_CONN);

	conn_mgr.flags = flags;
	conn_mgr.conn_avail = conn_avail;
	k_sem_give(&conn_mgr.state_lock);

	if (conn_mgr.cb_gap_state_changed != NULL) {
		conn_mgr.cb_gap_state_changed(flags, conn_avail);
	}
}

/* Possible state transition: the caller must have taken the lock. */
static int conn_mgr_adv_start(uint32_t *flags)
{
	/*
	 * - BT_LE_ADV_CONN_FAST_1: GAP recommends advertisers use this "when
	 *   user-initiated", won't resume the advertiser, and won't allow to reconnect
	 * - BT_LE_ADV_CONN_FAST_2: like BT_LE_ADV_CONN in Zephyr 3.6 and earlier,
	 *   but won't automatically resume the advertiser after it results in a connection
	 * - BT_LE_ADV_CONN_NAME: deprecated (Zephyr 3.7 and later), no longer even advertise
	 * - BT_LE_ADV_CONN: deprecated (Zephyr 3.7 and later),
	 *   but would resume the advertiser, and allow to reconnect
	 *
	 * See also: https://github.com/zephyrproject-rtos/zephyr/issues/31086
	 */
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, conn_mgr.adv_data, conn_mgr.adv_data_len,
				  conn_mgr.scan_data, conn_mgr.scan_data_len);
	if (err) {
		LOG_ERR("failed to start advertising: %d", err);

		*flags |= BME68X_GAP_STATE_ENETDOWN;
		return -ECONNREFUSED;
	}

	*flags &= ~BME68X_GAP_STATE_ENETDOWN;
	*flags |= BME68X_GAP_STATE_ADV_CONN;
	return 0;
}

int bme68x_esp_gap_init(struct bme68x_esp_gap_adv_cfg const *adv_cfg,
			bme68x_gap_state_changed_cb cb_gap_state_changed,
			struct bt_conn_auth_cb const *conn_auth_callbacks)
{
	int rc = bt_conn_cb_register(&conn_mgr_conn_callbacks);
	if (rc) {
		/* -EEXIST */
		LOG_ERR("already initialized ?");
		return rc;
	}

#ifdef CONFIG_BT_SMP
	if (conn_auth_callbacks != NULL) {
		(void)bt_conn_auth_cb_register(conn_auth_callbacks);
	}
#else
	ARG_UNUSED(conn_auth_callbacks);
#endif

	conn_manager_init(adv_cfg, CONN_MGR_CFG_FLAGS, cb_gap_state_changed);

	if (CONN_MGR_CFG_FLAGS & BME68X_GAP_CFG_ADV_AUTO) {
		rc = bme68x_esp_gap_adv_start();
	}

	return rc;
}

int bme68x_esp_gap_adv_start(void)
{
	uint32_t flags;
	uint8_t conn_avail;
	if (!conn_mgr_acquire_state(&flags, &conn_avail, K_NO_WAIT)) {
		return -EBUSY;
	}

	int rc = 0;

	if (flags & BME68X_GAP_STATE_ADV_CONN) {
		rc = -EADDRINUSE;
		goto finally;
	}

	if (!conn_avail) {
		rc = -ECONNREFUSED;
		goto finally;
	}

	rc = conn_mgr_adv_start(&flags);

finally:
	conn_mgr_release_state(flags, conn_avail);
	return rc;
}

static inline void conn_mgr_msgq_send(struct conn_mgr_msg const *msg)
{
	int err = k_msgq_put(&conn_mgr_msgq, msg, K_NO_WAIT);
	if (err) {
		LOG_ERR("message queue exhausted");
	}
}

/* bt_conn_cb::connected()
 *
 * API documentation:
 *
 *   If the connection was established from an advertising set then
 *   the advertising set cannot be restarted directly from this callback.
 *
 * Question: does the above only apply to LE extended advertising ?
 */
void cb_conn_connected(struct bt_conn *conn, uint8_t err)
{
	LE_ADDR_STR(bt_conn_get_dst(conn), addr_str);
	if (err) {
#ifdef CONFIG_BT_HCI_ERR_TO_STR
		LOG_WRN("failed connection %s: %s", addr_str, bt_hci_err_to_str(err));
#else
		LOG_WRN("failed connection %s: %d", addr_str, err);
#endif
	} else {
		LOG_INF("new connection: %s", addr_str);
	}

	/* Keep a reference until the consumer thread no longer needs the connection. */
	bt_conn_ref(conn);

	struct conn_mgr_msg msg = {.event = CONN_MGR_EVT_CONNECTION,
				   .connection = {.conn = conn, .err = err}};
	conn_mgr_msgq_send(&msg);
}

/* bt_conn_cb::disconnected()
 *
 * API documentation:
 *
 *   When this callback is called the stack still has one reference to the connection
 *   object. If the application in this callback tries to start either a connectable
 *   advertiser or create a new connection this might fail because there are no free
 *   connection objects available.
 *
 *   To avoid this issue it is recommended to either start connectable advertise or
 *   create a new connection using k_work_submit or increase CONFIG_BT_MAX_CONN.
 *
 * Questions:
 * - If "start connectable advertise" means BT_LE_ADV_CONN,
 *   then this might be a deprecated recommendation
 * - Using k_work_submit() here suggests we could assume that the connection
 *   will be recycled before the submitted work is executed
 */
void cb_conn_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LE_ADDR_STR(bt_conn_get_dst(conn), addr_str);
#ifdef CONFIG_BT_HCI_ERR_TO_STR
	LOG_INF("disconnected %s: %s", addr_str, bt_hci_err_to_str(reason));
#else
	LOG_INF("disconnected %s: %d", addr_str, reason);
#endif

	struct conn_mgr_msg const msg = {.event = CONN_MGR_EVT_DISCONNECTION,
					 .disconnection = {.conn = conn, .reason = reason}};
	conn_mgr_msgq_send(&msg);
}

/* bt_conn_cb::recycled()
 *
 * API documentation:
 *
 *   This callback notifies the application that it might be able to
 *   allocate a connection object. No guarantee, first come, first serve.
 *
 *   Use this to e.g. re-start connectable advertising or scanning.
 *
 *   Treat this callback as an ISR (originates from the BT stack).
 *   Making Bluetooth API calls in this context is error-prone and strongly
 *   discouraged.
 *
 * Questions:
 * - Isn't the above confusing ? How can we re-start advertising
 *   without "Making Bluetooth API calls" ?
 * - Why should this callback be treated differently than disconnected()
 *   which also runs on the sysworkq thread ?
 */
void cb_conn_recycled(void)
{
	LOG_DBG("connection recycled");

	conn_mgr_msgq_send(&(struct conn_mgr_msg){.event = CONN_MGR_EVT_CONN_RECYCLED});
}

void conn_mgr_msgq_recv(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct conn_mgr_msg msg;
	uint32_t flags;
	uint8_t conn_avail;

	while (1) {
		int rc = k_msgq_get(&conn_mgr_msgq, &msg, K_FOREVER);
		if (rc == -ENOMSG) {
			/* Queue purged. */
			continue;
		}

		/* Begin state transition. */
		if (!conn_mgr_acquire_state(&flags, &conn_avail, K_FOREVER)) {
			LOG_WRN("unexpected semaphore reset ?");
			continue;
		}

		if (msg.event == CONN_MGR_EVT_CONNECTION) {
			/* No longer advertising. */
			flags &= ~BME68X_GAP_STATE_ADV_CONN;
			/* A connection has been spent, even when finally failed. */
			--conn_avail;

			if (msg.connection.err) {
				/* Discard the reference added in the connected() callback. */
				bt_conn_unref(msg.disconnection.conn);
			} else {
				flags |= BME68X_GAP_STATE_CONNECTED;
			}

		} else if (msg.event == CONN_MGR_EVT_DISCONNECTION) {
			/* Return no longer used connection to pool. */
			bt_conn_unref(msg.disconnection.conn);

			if ((conn_avail + 1) == CONN_MGR_MAX_CONN) {
				/* conn_avail connections already recycled,
				 * plus this disconnection: all centrals have disconnected.
				 */
				flags &= ~BME68X_GAP_STATE_CONNECTED;
			}

		} else if (msg.event == CONN_MGR_EVT_CONN_RECYCLED) {
			/* Connection available for advertising. */
			++conn_avail;
		}

		if (!(flags & BME68X_GAP_STATE_ADV_CONN) && (flags & BME68X_GAP_CFG_ADV_AUTO)
		    && conn_avail) {
			/* Errors are flagged and logged. */
			(void)conn_mgr_adv_start(&flags);
		}

		/* Commit state transition. */
		conn_mgr_release_state(flags, conn_avail);
	}
}

#ifdef CONFIG_BT_SMP
void cb_conn_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	LE_ADDR_STR(bt_conn_get_dst(conn), addr_str);
	if (err) {
#ifdef CONFIG_BT_HCI_ERR_TO_STR
		LOG_WRN("failed security change %s: %s", addr_str, bt_hci_err_to_str(err));
#else
		LOG_WRN("failed security change %s: %d", addr_str, err);
#endif
	} else {
		LOG_INF("security changed %s: level %d", addr_str, level);
	}
}
#endif /* CONFIG_BT_SMP */
