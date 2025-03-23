/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BME68X_ESP_GAP_H_
#define BME68X_ESP_GAP_H_

/** @brief Connections management for the Environmental Sensing Profile.
 *
 * If `BME68X_ESP_GAP_ADV_AUTO` is set in Kconfig,
 * will automatically resume advertising when a connection resource is available.
 */

#include <stddef.h>
#include <stdint.h>
#include <zephyr/bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bitmask for configuration flags. */
#define BME68X_GAP_CFG_BITMASK (0x0000ffff)

/** Configuration flag, automatically resume advertising. */
#define BME68X_GAP_CFG_ADV_AUTO (1UL << 0)

/** Bitmask for state flags. */
#define BME68X_GAP_STATE_BITMASK (0xffff0000)

/** State flag, advertising connectable. */
#define BME68X_GAP_STATE_ADV_CONN (1UL << 16)

/** State flag, connected to at least one central. */
#define BME68X_GAP_STATE_CONNECTED (1UL << 17)

/** Bitmask for error flags (subset of state flags). */
#define BME68X_GAP_ERROR_BITMASK (0xff000000)

/** Error state flag, BLE advertising down. */
#define BME68X_GAP_STATE_ENETDOWN (1UL << 24)

/** Advertising configuration. */
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

/**
 * @brief Initialize connection management.
 *
 * Will start advertising if `BME68X_ESP_GAP_ADV_AUTO` is set in Kconfig.
 *
 * @param adv_cfg Advertising configuration.
 * @param flags Connection management configuration flags.
 * @param cb_state_changed Connection management state changes callback.
 *                         Invoked from the connection management thread, must not block.
 *                         May be `NULL`.
 *
 * @return 0 on success
 * @return -EEXIST if already initialized
 * @return another negative error code if starting advertising failed
 */
int bme68x_esp_gap_init(struct bme68x_esp_gap_adv_cfg const *adv_cfg, uint32_t flags,
			void (*cb_state_changed)(uint32_t flags, uint8_t conn_avail));

/**
 * @brief Start advertising.
 *
 * @return 0 On success (advertising connectable state)
 * @return -EBUSY Connection management task in progress
 * @return -EADDRINUSE Already in advertising connectable state
 * @return -ECONNREFUSED No free connection available
 *
 */
int bme68x_esp_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif // BME68X_ESP_GAP_H_
