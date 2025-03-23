/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_esp_sensor.h"
#include "bme68x_esp_gap.h"
#include "bme68x_ess.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_BT_SETTINGS
#include <zephyr/settings/settings.h>
#endif

LOG_MODULE_REGISTER(esp_sensor, CONFIG_BME68X_ESP_LOG_LEVEL);

static struct bt_data const sensor_adv_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
};
static struct bt_data const sensor_scan_data[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

#ifdef CONFIG_BME68X_ESP_GAP_ADV_AUTO
#define BME68X_ESP_GAP_CFG_FLAGS BME68X_GAP_CFG_ADV_AUTO
#else
#define BME68X_ESP_GAP_CFG_FLAGS 0
#endif

static int sensor_bt_init(void)
{
	/*
	 * Enable Bluetooth (synchronously).
	 *
	 * Note: generating lots of warnings is a known issue.
	 * See https://github.com/zephyrproject-rtos/zephyr/issues/80167.
	 */
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("BT subsystem unabailable: %d", err);
		return err;
	}

#ifdef CONFIG_BT_SETTINGS
	err = settings_load_subtree("bt");
	if (err) {
		LOG_ERR("failed to load BT settings: %d", err);
		return err;
	}
#endif

	return 0;
}

int bme68x_esp_sensor_init(void (*cb_state_changed)(uint32_t flags, uint8_t conn_avail))
{
	int err = sensor_bt_init();
	if (err) {
		return err;
	}

	err = bme68x_ess_init();
	if (err) {
		return err;
	}

	struct bme68x_esp_gap_adv_cfg adv_cfg = {.adv_data = sensor_adv_data,
						 .adv_data_len = ARRAY_SIZE(sensor_adv_data),
						 .scan_data = sensor_scan_data,
						 .scan_data_len = ARRAY_SIZE(sensor_scan_data)};

	err = bme68x_esp_gap_init(&adv_cfg, BME68X_ESP_GAP_CFG_FLAGS, cb_state_changed);
	return err;
}
