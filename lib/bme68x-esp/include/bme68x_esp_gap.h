/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BME68X_ESP_GAP_H_
#define BME68X_ESP_GAP_H_

/** @brief Simple connection manager for the Environmental Sensing Profile.
 *
 * Maintain a consistent state representing the bits we're interested in: is the sensor
 * advertising connectable, how many centrals are currently connected to the sensor, are there any
 * connections still available?
 *
 * Automatically resume advertising as soon as possible, allowing up to CONFIG_BT_MAX_CONN clients
 * to connect, disconnect and re-connect.
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Connections manager's state and configuration bit flags.
 *
 *  Bitmask for configuration flags:   0x0000ffff
 *  Bitmask for state flags:           0xffff0000
 *  Bitmask for state flags (errors):  0xff000000
 */
enum bme68x_esp_gap_flag {
	/** Configuration flag, automatically resume advertising. */
	BME68X_GAP_CFG_ADV_AUTO = 1UL << 0,
	/** State flag, advertising connectable. */
	BME68X_GAP_STATE_ADV_CONN = 1UL << 16,
	/** State flag, connected to at least one central. */
	BME68X_GAP_STATE_CONNECTED = 1UL << 17,
	/** Error flag, BLE advertising down. */
	BME68X_GAP_STATE_ENETDOWN = 1UL << 24,
};

/** @brief Advertising configuration.
 *
 *  Environmental Sensor role: shall advertise the Environmental Sensing Service,
 *  and optionally the Device Information Service and Battery Service.
 */
struct bme68x_esp_gap_adv_cfg {
	/** Data to be used in advertisement packets (services). */
	struct bt_data const *adv_data;
	/**  Number of elements in advertisement. */
	size_t adv_data_len;
	/** Data to be used in scan response packets (e.g. device name). */
	struct bt_data const *scan_data;
	/**  Number of elements in scan responses. */
	size_t scan_data_len;
};

/** @brief Callback to be informed of base connection management events.
 *
 * Applications can register this callback to e.g. blink an LED
 * when the peripheral is advertising connectable.
 *
 * @param flags      Configuration and state flags.
 * @param conn_avail Number of additional centrals that can still connect.
 */
typedef void (*bme68x_gap_state_changed_cb)(uint32_t flags, uint8_t conn_avail);

/** @brief Initialize connection management.
 *
 * Start advertising if BME68X_ESP_GAP_ADV_AUTO is set (Kconfig).
 *
 * @param adv_cfg              Advertising configuration.
 * @param cb_gap_state_changed Register this callback to be informed of base connection management
 *                             events. May be NULL.
 * @param conn_auth_callbacks  Authentication callbacks needed to update connections security
 *                             to Level 3 Encryption and authentication (MITM).
 *                             Leave NULL for JustWorks pairing or if Bluetooth SMP is disabled.
 *                             Authenticated connections require callbacks for at least
 *                             DisplayOnly I/O capabilities.
 *
 * @return 0 on success
 * @return -EEXIST if already initialized.
 * @return Another negative error code if starting advertising failed.
 */
int bme68x_esp_gap_init(struct bme68x_esp_gap_adv_cfg const *adv_cfg,
			bme68x_gap_state_changed_cb cb_gap_state_changed,
			struct bt_conn_auth_cb const *conn_auth_callbacks);

/** @brief Start advertising.
 *
 * @return 0 on success (advertising connectable state)
 * @return -EBUSY when a connection management task is in progress.
 *         Caller thread can yield and retry.
 * @return -EADDRINUSE Already in advertising connectable state.
 * @return -ECONNREFUSED No free connection available.
 */
int bme68x_esp_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif // BME68X_ESP_GAP_H_
