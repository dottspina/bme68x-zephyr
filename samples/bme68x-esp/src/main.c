/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BSEC-based Environmental Sensing Service (ESS).
 */

#include <drivers/bme68x_sensor_api.h>
#include "bme68x.h"
#include "bme68x_esp_gap.h"
#include "bme68x_esp_sensor.h"
#include "bme68x_ess.h"
#include "bme68x_iaq.h"

#include <stdint.h>

#include <zephyr/logging/log.h>
#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#endif

LOG_MODULE_REGISTER(app, CONFIG_BME68X_SAMPLE_LOG_LEVEL);

#ifdef CONFIG_BME68X_ES_TRIGGER_SETTINGS_WRITE_AUTHEN
/* Logging-based DisplayOnly I/O for authenticating connections.
 */
#define LE_CONN_ADDR_STR(CONN, S)                                                                  \
	char S[BT_ADDR_LE_STR_LEN];                                                                \
	bt_addr_le_to_str(bt_conn_get_dst(CONN), S, sizeof(S))

static void cb_auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	LE_CONN_ADDR_STR(conn, addr_str);
	LOG_INF("%s - Passkey: %06u", addr_str, passkey);
}
static void cb_auth_cancel(struct bt_conn *conn)
{
	LE_CONN_ADDR_STR(conn, addr_str);
	LOG_INF("%s - Authentication canceled", addr_str);
}
struct bt_conn_auth_cb const *conn_auth_callbacks = &(struct bt_conn_auth_cb){
	.passkey_display = cb_auth_passkey_display,
	.cancel = cb_auth_cancel,
};
#else
/* JustWorks pairing or Bluetooth SMP disabled. */
struct bt_conn_auth_cb const *conn_auth_callbacks = NULL;
#endif

static void iaq_output_handler(struct bme68x_iaq_sample const *iaq_sample);
static void cb_gap_state_changed(uint32_t flags, uint8_t conn_avail);

int main(void)
{
	/* Any compatible device will be fine. */
	struct device const *const dev = DEVICE_DT_GET_ONE(bosch_bme68x_sensor_api);

	struct bme68x_dev bme68x_dev = {0};
	int ret = bme68x_sensor_api_init(dev, &bme68x_dev);
	if (!ret) {
		ret = bme68x_init(&bme68x_dev);
	}
	if (ret) {
		LOG_ERR("BME68X initialization failed: %d", ret);
		return 0;
	}

	/* Initialize settings subsystem early. */
#ifdef CONFIG_SETTINGS
	ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("Settings subsystem error: %d", ret);
		return 0;
	}
#endif

	/* Initialize Bluetooth Environmental Sensor role (ESS). */
	ret = bme68x_esp_sensor_init(cb_gap_state_changed, conn_auth_callbacks);
	if (ret) {
		LOG_ERR("ESP initialization failed: %d", ret);
		return 0;
	}

	/* Initialize and configure BSEC IAQ. */
	ret = bme68x_iaq_init();
	if (ret) {
		LOG_ERR("IAQ initialization failed: %d", ret);
		return 0;
	}

	/* Start updating ESS Characteristics with BSEC algorithm output. */
	bme68x_iaq_run(&bme68x_dev, iaq_output_handler);

	return 0;
}

void iaq_output_handler(struct bme68x_iaq_sample const *iaq_sample)
{
	LOG_INF("%.02f degC, %.01f Pa, %.02f%%", (double)iaq_sample->temperature,
		(double)iaq_sample->raw_pressure, (double)iaq_sample->humidity);
	LOG_INF("VOC: %0.3f ppm", (double)iaq_sample->voc_equivalent);

	/* Update ESS Characteristics. */
	(void)bme68x_ess_update_temperature(iaq_sample->temperature * 100);
	(void)bme68x_ess_update_pressure(iaq_sample->raw_pressure * 10);
	(void)bme68x_ess_update_humidity(iaq_sample->humidity * 100);
}

void cb_gap_state_changed(uint32_t flags, uint8_t conn_avail)
{
	if (flags & BME68X_GAP_STATE_ADV_CONN) {
		/* E.g. turn LED on. */
		LOG_INF("advertising LED: ON");
	} else {
		/* E.g. turn LED off. */
		LOG_INF("advertising LED: OFF");
	}
}
