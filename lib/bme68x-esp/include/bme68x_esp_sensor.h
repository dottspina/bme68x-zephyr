/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BME68X_ESP_SENSOR_H_
#define BME68X_ESP_SENSOR_H_

/** @brief Environmental Sensor role of the Environmental Sensing Profile.
 *
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Environmental Sensor role.
 *
 * This API will load Bluetooth settings: the Settings subsystem shall be initialized.
 * This API will start advertising the Environmental Sensing Service
 * if `CONFIG_BME68X_ESP_GAP_ADV_AUTO` is set.
 *
 * @param cb_state_changed Connection management state changes callback.
 *                         Invoked from the connection management thread, must not block.
 *                         May be `NULL`.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int bme68x_esp_sensor_init(void (*cb_state_changed)(uint32_t flags, uint8_t conn_avail));

#ifdef __cplusplus
}
#endif

#endif // BME68X_ESP_SENSOR_H_
