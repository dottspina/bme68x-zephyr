/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BME68X_ESP_SENSOR_H_
#define BME68X_ESP_SENSOR_H_

/** @brief Environmental Sensor role of the Environmental Sensing Profile.
 *
 * Setup Bluetooth connections management
 * and initialize the Environmental Sensing Service (ESS).
 */

#include "bme68x_esp_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Environmental Sensor role.
 *
 * - Initialize Bluetooth host.
 * - Load bonding information from persistent storage if BT_SETTING is set (the Settings subsystem
 *   shall already be initialized).
 * - Initialize the Environmental Sensing Service (ESS),
 *   start advertising if BME68X_ESP_GAP_ADV_AUTO is set
 *
 * @param cb_gap_state_changed Register this callback to be informed of base connection management
 *                             events. May be NULL.
 * @param conn_auth_callbacks  Authentication callbacks needed to update connections security
 *                             to Level 3 Encryption and authentication (MITM).
 *                             Leave NULL for JustWorks pairing or if Bluetooth SMP is disabled.
 *                             Authenticated connections require callbacks for at least
 *                             DisplayOnly I/O capabilities.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int bme68x_esp_sensor_init(bme68x_gap_state_changed_cb cb_gap_state_changed,
			   struct bt_conn_auth_cb const *conn_auth_callbacks);

#ifdef __cplusplus
}
#endif

#endif // BME68X_ESP_SENSOR_H_
