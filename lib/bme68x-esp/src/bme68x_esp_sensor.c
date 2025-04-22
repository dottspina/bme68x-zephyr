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

static int sensor_bt_init(void)
{
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

int bme68x_esp_sensor_init(bme68x_gap_state_changed_cb cb_gap_state_changed,
			   struct bt_conn_auth_cb const *conn_auth_callbacks)
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

	err = bme68x_esp_gap_init(&adv_cfg, cb_gap_state_changed, conn_auth_callbacks);
	return err;
}
