/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_ess.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "zephyr/bluetooth/att.h"
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <stdbool.h>
#include <sys/types.h>

LOG_MODULE_REGISTER(esp_ess, CONFIG_BME68X_ESP_LOG_LEVEL);

#define ESS_GATT_TEMPERATURE_UNKNOWN ((int16_t)0x8000)
#define ESS_GATT_HUMIDITY_UNKNOWN    ((uint16_t)0xffff)

/*  BT SIG Assigned Numbers §2.4.1,
 *  Characteristic Presentation Format, GATT Format Types.
 */
#define CPF_FORMAT_UINT16 0x06
#define CPF_FORMAT_UINT32 0x08
#define CPF_FORMAT_SINT16 0x0e

/* Client attempts to write an ES Trigger Setting Condition
 * value that is RFU.
 * ESS §1.6 Application Error codes.
 */
#define ESS_ERROR_CONDITION_NOT_SUPPORTED 0x81

/* ESS §3.1.2 Characteristic Descriptors,
 * Table 3.2: Environmental Sensing Service Characteristic Descriptors:
 * ES Trigger Setting: If Write is supported, bonding is mandatory.
 *
 * ESS §3.1.2.3.1 ES Trigger Setting Descriptor and ES Configuration Descriptor Behavior:
 *   Bonding is mandatory if the ES Trigger Setting descriptor and ES Configuration descriptors
 *   are writable by the Client.
 *   Therefore, writing to the ES Trigger Setting and ES Configuration descriptors shall be
 *   subject to authorization as follows: If the Client is a bonded Client and these descriptors
 *   are writable, the Client shall be granted authorization to write to these descriptors.
 *
 *   If the Server allows the Client to control the conditions under which data is notified
 *   (i.e.,the ES Trigger Setting descriptor and ES Configuration descriptor, if present,
 *   are writable by the Client), it shall allow separate control for each bonded Client
 *   and therefore shall retain a separate value of these descriptors per bond.
 *
 * Note: we don't support separate control of the descriptors for each bonded client.
 */
#ifdef CONFIG_BME68X_ES_TRIGGER_SETTINGS_WRITE
/* ES Trigger Settings are world writable. */
#define ES_TRIGGER_SETTING_PERM (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
#elif CONFIG_BME68X_ES_TRIGGER_SETTINGS_WRITE_ENCRYPT
/* ES Trigger Settings are writable over encrypted connections. */
#define ES_TRIGGER_SETTING_PERM (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT)
#elif CONFIG_BME68X_ES_TRIGGER_SETTINGS_WRITE_AUTHEN
/* ES Trigger Settings are writable over authenticated connections. */
#define ES_TRIGGER_SETTING_PERM (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_AUTHEN)
#else
/* ES Trigger Settings are read-only by default. */
#define ES_TRIGGER_SETTING_PERM BT_GATT_PERM_READ
#endif

/* ESS characteristic state. */
struct ess_characteristic {

	/* Characteristic Presentation Format (Assigned Numbers §2.4). */
	struct bt_gatt_cpf const cpf;

	/* Attribute value, signedness and size depend on characteristic (up to 32-bit).
	 */
	atomic_t value;

	/* Attribute value timestamp used for the ES Trigger Setting
	 * condition ES_TRIGGER_GTE_TIME (32-bit unsigned, seconds since system boot).
	 */
	uint32_t value_ts;

	/* Whether there's at least one connected peer that has enabled
	 * notifications in its CCC for this characteristic.
	 */
	atomic_t ccc_notify;

	/* ES Trigger Setting descriptor for this characteristic.
	 */
	struct es_trigger_setting trigger_setting;

	/* Used when the trigger setting condition is a fixed time interval
	 * between transmissions.
	 */
	struct k_timer trigger_timer;

	/* Represent the number of received GATT requests writing to
	 * this characteristic's ES Trigger Setting, including:
	 * - the requests already completed
	 * - plus possibly an ongoing request
	 *
	 * Allows the thread which updates the ESS Characteristic values to know
	 * whether it has been preempted by a thread that invalidated its use
	 * of the ES Trigger Setting.
	 */
	atomic_t trigger_setting_cnt;
};

/* GATT read ESS characteristic's value (16-bit integer). */
static ssize_t ess_chrc_gatt_read16(struct ess_characteristic const *chrc, struct bt_conn *conn,
				    struct bt_gatt_attr const *attr, void *buf, uint16_t len,
				    uint16_t offset)
{
	uint8_t value_le[2];
	sys_put_le16(atomic_get(&chrc->value), value_le);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value_le, sizeof(value_le));
}

/* GATT read ESS characteristic's value (32-bit integer). */
static ssize_t ess_chrc_gatt_read32(struct ess_characteristic const *chrc, struct bt_conn *conn,
				    struct bt_gatt_attr const *attr, void *buf, uint16_t len,
				    uint16_t offset)
{
	uint8_t value_le[4];
	sys_put_le32(atomic_get(&chrc->value), value_le);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value_le, sizeof(value_le));
}

/* GATT read callback for ESS Characteristic's value.
 *
 * Implements bt_gatt_attr_read_func_t().
 *
 * Runs on BT RX WQ.
 */
static ssize_t ess_chrc_gatt_read_cb(struct bt_conn *conn, struct bt_gatt_attr const *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	atomic_t const *value = attr->user_data;
	struct ess_characteristic const *chrc = CONTAINER_OF(value, struct ess_characteristic,
							     value);

	switch (chrc->cpf.format) {
	case CPF_FORMAT_SINT16:
		__fallthrough;
	case CPF_FORMAT_UINT16:
		return ess_chrc_gatt_read16(chrc, conn, attr, buf, len, offset);
	case CPF_FORMAT_UINT32:
		return ess_chrc_gatt_read32(chrc, conn, attr, buf, len, offset);
	default:
		LOG_WRN("unexpected CPF %u", chrc->cpf.format);
	}
	return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

/* Forward declaration of ESS characteristic's value attribute lookup,
 * implementation depends on ess_srv service instance.
 */
static struct bt_gatt_attr const *ess_chrc_attr_find(struct ess_characteristic const *chrc);

/* Notify prepared buffer as ESS characteristic's value. */
static void ess_chrc_gatt_notify(struct ess_characteristic const *chrc, void const *buf,
				 uint16_t len)
{
	struct bt_gatt_attr const *attr = ess_chrc_attr_find(chrc);
	if (attr == NULL) {
		return;
	}

	/* We may get -ENOTCONN if notifications have been disabled by a CCC changed event
	 * since we decided to notify this characteristic's value.
	 * This is not considered an error, and the warning message bellow should not repeat.
	 */
	int const err = bt_gatt_notify(NULL, attr, buf, len);

	char uuid_str[BT_UUID_STR_LEN];
	bt_uuid_to_str(attr->uuid, uuid_str, sizeof(uuid_str));

	if (err) {
		LOG_WRN("%s: notifications error %d", uuid_str, err);
	} else {
		LOG_DBG("%s", uuid_str);
	}
}

/* GATT notify ESS characteristic's value (16-bit integer). */
static void ess_chrc_gatt_notify16(struct ess_characteristic const *chrc)
{
	atomic_val_t const value = atomic_get(&chrc->value);

	uint8_t value_le[2];
	sys_put_le16(value, value_le);
	ess_chrc_gatt_notify(chrc, value_le, sizeof(value_le));
}

/* GATT notify ESS characteristic's value (32-bit integer). */
static void ess_chrc_gatt_notify32(struct ess_characteristic const *chrc)
{
	atomic_val_t const value = atomic_get(&chrc->value);

	uint8_t value_le[4];
	sys_put_le32(value, value_le);
	ess_chrc_gatt_notify(chrc, value_le, sizeof(value_le));
}

/* Notify ESS characteristic's value. */
static void ess_chrc_notify_value(struct ess_characteristic const *chrc)
{
	switch (chrc->cpf.format) {
	case CPF_FORMAT_SINT16:
		__fallthrough;
	case CPF_FORMAT_UINT16:
		ess_chrc_gatt_notify16(chrc);
		break;
	case CPF_FORMAT_UINT32:
		ess_chrc_gatt_notify32(chrc);
		break;
	default:
		LOG_WRN("unexpected CPF %u", chrc->cpf.format);
	}
}

/* GATT read ESS characteristic's trigger setting (no operand). */
static ssize_t es_trigger_setting_gatt_read_operand_na(struct es_trigger_setting const *setting,
						       struct bt_conn *conn,
						       struct bt_gatt_attr const *attr, void *buf,
						       uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1] = {setting->condition};
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (time-based, uint24). */
static ssize_t es_trigger_setting_gatt_read_seconds(struct es_trigger_setting const *setting,
						    struct bt_conn *conn,
						    struct bt_gatt_attr const *attr, void *buf,
						    uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + 3] = {setting->condition};
	sys_put_le24(setting->operand.seconds, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, sint16). */
static ssize_t es_trigger_setting_gatt_read_sint16(struct es_trigger_setting const *setting,
						   struct bt_conn *conn,
						   struct bt_gatt_attr const *attr, void *buf,
						   uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + 2] = {setting->condition};
	sys_put_le16(setting->operand.val_sint16, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, uint16). */
static ssize_t es_trigger_setting_gatt_read_uint16(struct es_trigger_setting const *setting,
						   struct bt_conn *conn,
						   struct bt_gatt_attr const *attr, void *buf,
						   uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + 2] = {setting->condition};
	sys_put_le16(setting->operand.val_uint16, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, uint32). */
static ssize_t es_trigger_setting_gatt_read_uint32(struct es_trigger_setting const *setting,
						   struct bt_conn *conn,
						   struct bt_gatt_attr const *attr, void *buf,
						   uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + 4] = {setting->condition};
	sys_put_le32(setting->operand.val_uint32, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read callback for ES Trigger Setting descriptors.
 *
 * Implements bt_gatt_attr_read_func_t().
 *
 * Runs on BT RX WQ.
 */
static ssize_t es_trigger_setting_gatt_read_cb(struct bt_conn *conn,
					       struct bt_gatt_attr const *attr, void *buf,
					       uint16_t len, uint16_t offset)
{
	struct es_trigger_setting const *trigger_setting = attr->user_data;
	struct ess_characteristic const *chrc = CONTAINER_OF(
		trigger_setting, struct ess_characteristic, trigger_setting);

	switch (trigger_setting->condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		return es_trigger_setting_gatt_read_operand_na(&chrc->trigger_setting, conn, attr,
							       buf, len, offset);
	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		return es_trigger_setting_gatt_read_seconds(&chrc->trigger_setting, conn, attr, buf,
							    len, offset);
	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		switch (chrc->cpf.format) {
		case CPF_FORMAT_SINT16:
			return es_trigger_setting_gatt_read_sint16(&chrc->trigger_setting, conn,
								   attr, buf, len, offset);
		case CPF_FORMAT_UINT16:
			return es_trigger_setting_gatt_read_uint16(&chrc->trigger_setting, conn,
								   attr, buf, len, offset);
		case CPF_FORMAT_UINT32:
			return es_trigger_setting_gatt_read_uint32(&chrc->trigger_setting, conn,
								   attr, buf, len, offset);
		default:
			LOG_WRN("unexpected CPF %u", chrc->cpf.format);
		}
		break;

	default:
		LOG_WRN("unsupported ES Trigger Setting condition %d", trigger_setting->condition);
	}

	return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

/* Resume periodic notifications for ESS characteristic:
 * - stop when notifications disabled by a CCC changed event,
 *   or when initiating an ES Trigger Setting reconfiguration (GATT write)
 * - start when notifications enabled by a CCC changed event,
 *   or after an ES Trigger Setting reconfiguration (GATT write)
 */
static void ess_chrc_trigger_timer_resume(struct ess_characteristic *chrc, bool ccc_notify)
{
	if (ccc_notify) {
		/* Start periodic notifications. */
		k_timeout_t const timeout = K_SECONDS(chrc->trigger_setting.operand.seconds);
		k_timer_start(&chrc->trigger_timer, timeout, timeout);

		LOG_DBG("CCC timer start (%u secs)", chrc->trigger_setting.operand.seconds);

	} else {
		/* Stop periodic notifications. */
		k_timer_stop(&chrc->trigger_timer);

		LOG_DBG("CCC timer stop");
	}
}

/* Polymorphic test of value-based trigger setting conditions.
 *
 * COND: condition type
 * OPERAND: condition's operand value
 * V: value to check the condition with
 * V0: old value (used for ES_TRIGGER_VALUE_CHANGED)
 */
#define ES_TRIGGER_SETTING_CHECK_VALUE(COND, OPERAND, V, V0)                                       \
	((COND) == ES_TRIGGER_VALUE_CHANGED         ? (V) != (V0)                                  \
	 : (COND) == ES_TRIGGER_LT_VALUE            ? (V) < (OPERAND)                              \
	 : (COND) == ES_TRIGGER_LTE_VALUE           ? (V) <= (OPERAND)                             \
	 : (COND) == ES_TRIGGER_GT_VALUE            ? (V) > (OPERAND)                              \
	 : (COND) == ES_TRIGGER_GTE_VALUE           ? (V) >= (OPERAND)                             \
	 : (COND) == ES_TRIGGER_SPECIFIED_VALUE     ? (V) == (OPERAND)                             \
	 : (COND) == ES_TRIGGER_NOT_SPECIFIED_VALUE ? (V) != (OPERAND)                             \
						    : false)

/* Check the format of an ES Trigger Setting operand assuming given condition and length.
 * Returns the size of the expected operand, or a negative value on error.
 */
static inline ssize_t es_trigger_setting_chk_format(uint8_t cpf,
						    enum es_trigger_setting_condition condition,
						    uint16_t len)
{
	/* ESS §3.1.2.3.1 ES Trigger Setting Descriptor and ES Configuration Descriptor Behavior:
	 * If the Client attempts to write an Operand to the ES Trigger Setting descriptor that is
	 * outside of the operating range of the Server (refer to Section 3.1.2.5) or otherwise
	 * improperly formatted, the Server shall respond with the Out of Range Error Code.
	 */
	switch (condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		if (len == 0) {
			return 0;
		} else {
			LOG_WRN("no operand expected for condition %d", condition);
			return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
		}
	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		/* Seconds UINT24. */
		if (len == 3) {
			return 3;
		} else {
			LOG_WRN("24-bit operand expected (condition %d)", condition);
			return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
		}
	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		switch (cpf) {
		case CPF_FORMAT_SINT16:
			__fallthrough;
		case CPF_FORMAT_UINT16:
			if (len == 2) {
				return 2;
			} else {
				LOG_WRN("16-bit operand expected (condition %d)", condition);
				return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
			}
		case CPF_FORMAT_UINT32:
			if (len == 4) {
				return 4;
			} else {
				LOG_WRN("32-bit operand expected (condition %d)", condition);
				return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
			}
		default:
			LOG_WRN("unexpected CPF format %u", cpf);
		}

	default:
		LOG_WRN("unsupported ES Trigger Setting condition %d", condition);
		/* ESS §3.1.2.3.1 ES Trigger Setting Descriptor and ES Configuration Descriptor
		 * Behavior: If the ES Trigger Setting descriptor is writable and a Client attempts
		 * to write a Condition Value that is RFU, the Server shall respond with the
		 * Condition not supported Error Code.
		 */
		return BT_GATT_ERR(ESS_ERROR_CONDITION_NOT_SUPPORTED);
	}

	return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

/* GATT write callback for ES Trigger Setting descriptors.
 *
 * Implements bt_gatt_attr_write_func_t().
 *
 * Runs on BT RX WQ.
 */
static ssize_t es_trigger_setting_gatt_write_cb(struct bt_conn *conn,
						struct bt_gatt_attr const *attr, void const *buf,
						uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(flags);

	struct es_trigger_setting *trigger_setting = attr->user_data;
	struct ess_characteristic *chrc = CONTAINER_OF(trigger_setting, struct ess_characteristic,
						       trigger_setting);

	ssize_t const sz_pdu = len - offset;
	if (sz_pdu < 1) {
		LOG_DBG("skipping empty PDU");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	uint8_t const *pdu = ((uint8_t *)buf) + offset;
	enum es_trigger_setting_condition const condition = pdu[0];

	ssize_t const sz_operand = es_trigger_setting_chk_format(chrc->cpf.format, condition,
								 sz_pdu - 1);
	if (sz_operand < 0) {
		return sz_operand;
	}

	/* Stop fixed interval notifications. */
	if (trigger_setting->condition == ES_TRIGGER_FIXED_TIME) {
		ess_chrc_trigger_timer_resume(chrc, false);
	}

	/* Tell any preempted reader thread that we're updating
	 * the ES Trigger Setting descriptor.
	 */
	(void)atomic_inc(&chrc->trigger_setting_cnt);

	trigger_setting->condition = condition;
	uint8_t const *operand = pdu + 1;

	LOG_DBG("condition: %u", trigger_setting->condition);
	switch (trigger_setting->condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		break;
	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		/* Seconds UINT24. */
		trigger_setting->operand.seconds = sys_get_le24(operand);
		LOG_DBG("operand: %u", trigger_setting->operand.seconds);
		break;
	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		switch (chrc->cpf.format) {
		case CPF_FORMAT_SINT16:
			trigger_setting->operand.val_sint16 = sys_get_le16(operand);
			LOG_DBG("operand: %d", trigger_setting->operand.val_sint16);
			break;
		case CPF_FORMAT_UINT16:
			trigger_setting->operand.val_uint16 = sys_get_le16(operand);
			LOG_DBG("operand: %u", trigger_setting->operand.val_uint16);
			break;
		case CPF_FORMAT_UINT32:
			trigger_setting->operand.val_uint32 = sys_get_le32(operand);
			LOG_DBG("operand: %u", trigger_setting->operand.val_uint32);
			break;
		default:
			/* Should not happen, format already checked. */
			;
		}

	default:
		/* Should not happen, condition already checked. */
		;
	}

	if (trigger_setting->condition == ES_TRIGGER_FIXED_TIME
	    && (atomic_get(&chrc->ccc_notify) != 0)) {
		/* Resume notifications with new fixed interval. */
		ess_chrc_trigger_timer_resume(chrc, true);
	}

	return sz_pdu;
}

/* Notify an ESS Characteristic's value update according to its ES Trigger Setting descriptor.
 *
 * If the new value is notified after evaluating an ES_TRIGGER_GTE_TIME condition,
 * the value's timestamp is also updated.
 */
static void ess_chrc_value_update_notify(struct ess_characteristic *chrc, void *value,
					 void *old_value)
{
	if (atomic_get(&chrc->ccc_notify) == 0) {
		/* Notifications are disabled in the Client Characteristic Configuration descriptor
		 * of all connected centrals.
		 */
		return;
	}

	/* We rely on an atomic counter to determine if we've been preempted by
	 * a GATT write request while reading the ES Trigger Setting descriptor.
	 */
	enum es_trigger_setting_condition condition;
	union es_trigger_setting_operand operand;
	atomic_val_t writes_cnt;
	do {
		writes_cnt = atomic_get(&chrc->trigger_setting_cnt);
		condition = chrc->trigger_setting.condition;
		operand = chrc->trigger_setting.operand;
	} while (atomic_get(&chrc->trigger_setting_cnt) != writes_cnt);

	/* Non-zero only for the ES_TRIGGER_GTE_TIME condition. */
	uint32_t timesptamp = 0;
	bool notify = false;

	switch (condition) {
	case ES_TRIGGER_INACTIVE:
		/* Per ESS §3.1.2.3.1, an inactive trigger disables notifications. */
		break;
	case ES_TRIGGER_FIXED_TIME:
		/* Periodic notifications are not triggered by value updates. */
		break;
	case ES_TRIGGER_GTE_TIME:
		timesptamp = k_uptime_seconds();
		notify = (timesptamp - chrc->value_ts) >= operand.seconds;
		break;
	case ES_TRIGGER_VALUE_CHANGED:
		__fallthrough;
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		switch (chrc->cpf.format) {
		case CPF_FORMAT_UINT16:
			notify = ES_TRIGGER_SETTING_CHECK_VALUE(condition, operand.val_uint16,
								*((uint16_t *)value),
								*((uint16_t *)old_value));
			break;
		case CPF_FORMAT_UINT32:
			notify = ES_TRIGGER_SETTING_CHECK_VALUE(condition, operand.val_uint32,
								*((uint32_t *)value),
								*((uint32_t *)old_value));
			break;
		case CPF_FORMAT_SINT16:
			notify = ES_TRIGGER_SETTING_CHECK_VALUE(condition, operand.val_sint16,
								*((int16_t *)value),
								*((int16_t *)old_value));
			break;
		default:
			LOG_WRN("unexpected CPF format %u", chrc->cpf.format);
		}
		break;

	default:
		LOG_WRN("undefined ES Trigger Setting condition %d", condition);
	}

	if (notify) {
		if (timesptamp) {
			/* Update value's timestamp only when the notification
			 * is triggered by an ES_TRIGGER_GTE_TIME condition.
			 */
			chrc->value_ts = timesptamp;
		}
		ess_chrc_notify_value(chrc);
	}
}

/* CCC changed event, value of BT_GATT_CCC_NOTIFY means that at least
 * one connected peer has notifications enabled for this characteristic.
 */
static void ess_chrc_ccc_changed(struct ess_characteristic *chrc, uint16_t value)
{
	bool ccc_notify = value == BT_GATT_CCC_NOTIFY;
	atomic_set(&chrc->ccc_notify, ccc_notify);

	if (chrc->trigger_setting.condition == ES_TRIGGER_FIXED_TIME) {
		ess_chrc_trigger_timer_resume(chrc, ccc_notify);
	}
}

/* Handle a GATT event changing whether at least one central is subscribed
 * to an ESS Characteristic.
 *
 * Runs on BT RX WQ.
 */
static void ess_chrc_gatt_ccc_changed_cb(struct bt_gatt_attr const *attr, uint16_t value)
{
	/* The ESS characteristic's value attribute immediately precedes
	 * its CCC descriptor attribute.
	 */
	struct bt_gatt_attr const *chrc_attr = attr - 1;

	/* Lookup our ESS characteristic for this attribute. */
	atomic_t *chrc_value = chrc_attr->user_data;
	struct ess_characteristic *chrc = CONTAINER_OF(chrc_value, struct ess_characteristic,
						       value);

	char uuid_str[BT_UUID_STR_LEN];
	bt_uuid_to_str(chrc_attr->uuid, uuid_str, sizeof(uuid_str));
	LOG_DBG("%s: %#x", uuid_str, value);

	ess_chrc_ccc_changed(chrc, value);
}

/* Periodic notification, no configuration nor state to check. */
static void ess_chrc_trigger_setting_timeout(struct k_timer *timer)
{
	struct ess_characteristic const *chrc = CONTAINER_OF(timer, struct ess_characteristic,
							     trigger_timer);
	ess_chrc_notify_value(chrc);
}

/* Polymorphic static initializer for ESS characteristics' trigger settings.
 *
 * CHRC: Pointer to struct ess_characteristic
 * COND: Condition type
 * OPERAND: Operand, represent the seconds for time-based conditions,
 *          or the specified value for value-based conditions
 */
#define ESS_CHRC_TRIGGER_SETTING_INIT(CHRC, COND, OPERAND)                                         \
	do {                                                                                       \
		k_timer_init(&(CHRC)->trigger_timer, ess_chrc_trigger_setting_timeout, NULL);      \
                                                                                                   \
		(CHRC)->trigger_setting.condition = (COND);                                        \
		switch (COND) {                                                                    \
		/* Time-based conditions. */                                                       \
		case ES_TRIGGER_FIXED_TIME:                                                        \
			__fallthrough;                                                             \
		case ES_TRIGGER_GTE_TIME:                                                          \
			(CHRC)->trigger_setting.operand.seconds = (OPERAND);                       \
			break;                                                                     \
		/* Value-based conditions. */                                                      \
		case ES_TRIGGER_LT_VALUE:                                                          \
			__fallthrough;                                                             \
		case ES_TRIGGER_LTE_VALUE:                                                         \
			__fallthrough;                                                             \
		case ES_TRIGGER_GT_VALUE:                                                          \
			__fallthrough;                                                             \
		case ES_TRIGGER_GTE_VALUE:                                                         \
			__fallthrough;                                                             \
		case ES_TRIGGER_SPECIFIED_VALUE:                                                   \
			__fallthrough;                                                             \
		case ES_TRIGGER_NOT_SPECIFIED_VALUE:                                               \
			switch ((CHRC)->cpf.format) {                                              \
			case CPF_FORMAT_SINT16:                                                    \
				(CHRC)->trigger_setting.operand.val_sint16 = (OPERAND);            \
				break;                                                             \
			case CPF_FORMAT_UINT16:                                                    \
				(CHRC)->trigger_setting.operand.val_uint16 = (OPERAND);            \
				break;                                                             \
			case CPF_FORMAT_UINT32:                                                    \
				(CHRC)->trigger_setting.operand.val_uint32 = (OPERAND);            \
				break;                                                             \
			default:                                                                   \
				LOG_ERR("unexpected CPF %u", (CHRC)->cpf.format);                  \
			}                                                                          \
			break;                                                                     \
		/* Conditions without operand. */                                                  \
		case ES_TRIGGER_INACTIVE:                                                          \
			__fallthrough;                                                             \
		case ES_TRIGGER_VALUE_CHANGED:                                                     \
			break;                                                                     \
		default:                                                                           \
			LOG_ERR("invalid Trigger Setting condition %u", COND);                     \
		}                                                                                  \
	} while (false);

/* Static definition of an ESS Characteristic to be used with BT_GATT_SERVICE_DEFINE.
 *
 * All ESS characteristics share the same GATT callbacks:
 * - ess_chrc_gatt_read_cb(): request reading the characteristic's value
 * - ess_chrc_gatt_ccc_changed_cb(): received event changing whether there's at least
 *                                   one central subscribed to the characteristic
 * - es_trigger_setting_gatt_read_cb(): request reading the ES Trigger Setting descriptor
 * - es_trigger_setting_gatt_write_cb(): request writing to the ES Trigger Setting descriptor
 *
 * All these callbacks run sequentially on the BT RW WQ thread.
 *
 * UUID: ESS Characteristic UUID
 * CHRC: Pointer to the internal ESS Characteristic implementation (state and configuration)
 */
#define ESS_CHARACTERISTIC(UUID, CHRC)                                                             \
	BT_GATT_CHARACTERISTIC((UUID), BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, \
			       ess_chrc_gatt_read_cb, NULL, &(CHRC)->value),                       \
		BT_GATT_CCC(ess_chrc_gatt_ccc_changed_cb, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), \
		BT_GATT_CPF(&(CHRC)->cpf),                                                         \
		BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING, ES_TRIGGER_SETTING_PERM,            \
				   es_trigger_setting_gatt_read_cb,                                \
				   es_trigger_setting_gatt_write_cb, &(CHRC)->trigger_setting)

static struct ess_characteristic ess_chrc_temperature = {
	/* Client Presentation Format - Temperature (GATT_SS§3.218). */
	.cpf =
		{
			/* AN§2.4.1 GATT Format Types: sint16. */
			.format = CPF_FORMAT_SINT16,
			/* Represented values: M = 1, d = -2, b = 0 */
			.exponent = -2,
			/* AN§3.5 Units: Celsius temperature (degree Celsius). */
			.unit = 0x272F,
			/* Bluetooth SIG */
			.name_space = 0x01,
			/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main".
			 */
			.description = 0x0106,
		},
	.value = ATOMIC_INIT(ESS_GATT_TEMPERATURE_UNKNOWN),
	.trigger_setting = {.condition = ES_TRIGGER_VALUE_CHANGED},
};

static struct ess_characteristic ess_chrc_pressure = {
	/* Client Presentation Format - Pressure (GATT_SS§3.181). */
	.cpf =
		{
			/* AN§2.4.1 GATT Format Types: uint32. */
			.format = CPF_FORMAT_UINT32,
			/* Represented values: M = 1, d = -1, b = 0 */
			.exponent = -1,
			/* AN§3.5 Units: Pressure (Pascal). */
			.unit = 0x2724,
			/* Bluetooth SIG. */
			.name_space = 0x01,
			/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main".
			 */
			.description = 0x0106,
		},
	.trigger_setting = {.condition = ES_TRIGGER_VALUE_CHANGED},
};

static struct ess_characteristic ess_chrc_humidity = {
	/* Client Presentation Format - Humidity (GATT_SS§3.124). */
	.cpf =
		{
			/* AN§2.4.1 GATT Format Types: uint16. */
			.format = CPF_FORMAT_UINT16,
			/* Represented values: M = 1, d = -2, b = 0 */
			.exponent = -2,
			/* AN§3.5 Units: Percentage. */
			.unit = 0x27AD,
			/* Bluetooth SIG. */
			.name_space = 0x01,
			/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main".
			 */
			.description = 0x0106,
		},
	.value = ATOMIC_INIT(ESS_GATT_HUMIDITY_UNKNOWN),
	.trigger_setting = {.condition = ES_TRIGGER_VALUE_CHANGED},
};

BT_GATT_SERVICE_DEFINE(ess_srv, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
		       ESS_CHARACTERISTIC(BT_UUID_TEMPERATURE, &ess_chrc_temperature),
		       ESS_CHARACTERISTIC(BT_UUID_PRESSURE, &ess_chrc_pressure),
		       ESS_CHARACTERISTIC(BT_UUID_HUMIDITY, &ess_chrc_humidity));

int bme68x_ess_init(void)
{
	ESS_CHRC_TRIGGER_SETTING_INIT(&ess_chrc_temperature,
				      CONFIG_BME68X_TEMPERATURE_TRIGGER_CONDITION,
				      CONFIG_BME68X_TEMPERATURE_TRIGGER_OPERAND);

	ESS_CHRC_TRIGGER_SETTING_INIT(&ess_chrc_pressure, CONFIG_BME68X_PRESSURE_TRIGGER_CONDITION,
				      CONFIG_BME68X_PRESSURE_TRIGGER_OPERAND);

	ESS_CHRC_TRIGGER_SETTING_INIT(&ess_chrc_humidity, CONFIG_BME68X_HUMIDITY_TRIGGER_CONDITION,
				      CONFIG_BME68X_HUMIDITY_TRIGGER_OPERAND);

	LOG_INF("Temperature trigger: 0x%02x (%d)", CONFIG_BME68X_TEMPERATURE_TRIGGER_CONDITION,
		CONFIG_BME68X_TEMPERATURE_TRIGGER_OPERAND);
	LOG_INF("Pressure trigger: 0x%02x (%u)", CONFIG_BME68X_PRESSURE_TRIGGER_CONDITION,
		CONFIG_BME68X_PRESSURE_TRIGGER_OPERAND);
	LOG_INF("Humidity trigger: 0x%02x (%u)", CONFIG_BME68X_HUMIDITY_TRIGGER_CONDITION,
		CONFIG_BME68X_HUMIDITY_TRIGGER_OPERAND);

	return 0;
}

int bme68x_ess_update_temperature(int16_t temperature)
{
	if ((temperature < -27315) && (temperature != ESS_GATT_TEMPERATURE_UNKNOWN)) {
		LOG_WRN("invalid temperature: %d", temperature);
		return -EINVAL;
	}

	int16_t old_value = atomic_set(&ess_chrc_temperature.value, temperature);
	ess_chrc_value_update_notify(&ess_chrc_temperature, &temperature, &old_value);
	return 0;
}

int bme68x_ess_update_pressure(uint32_t pressure)
{
	uint32_t old_value = atomic_set(&ess_chrc_pressure.value, pressure);
	ess_chrc_value_update_notify(&ess_chrc_pressure, &pressure, &old_value);
	return 0;
}

int bme68x_ess_update_humidity(uint16_t humidity)
{
	if ((humidity > 10000) && (humidity != ESS_GATT_HUMIDITY_UNKNOWN)) {
		LOG_WRN("invalid humidity: %u", humidity);
		return -EINVAL;
	}

	uint16_t old_value = atomic_set(&ess_chrc_humidity.value, humidity);
	ess_chrc_value_update_notify(&ess_chrc_humidity, &humidity, &old_value);
	return 0;
}

struct bt_gatt_attr const *ess_chrc_attr_find(struct ess_characteristic const *chrc)
{
	for (size_t i = 0; i < ess_srv.attr_count; i++) {
		if (ess_srv.attrs[i].user_data == &chrc->value) {
			return &ess_srv.attrs[i];
		}
	}
	LOG_ERR("ESS characteristic not found %p", (void *)chrc);
	return NULL;
}
