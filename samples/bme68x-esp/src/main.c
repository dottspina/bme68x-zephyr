/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Environmental Sensing Service with BSEC and the BME68X Sensor API.
 */

#include <drivers/bme68x_sensor_api.h>
#include "bme68x.h"
#include "bme68x_iaq.h"
#include "bme68x_ess.h"
#include "bme68x_esp_sensor.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(app, CONFIG_BME68X_SAMPLE_LOG_LEVEL);

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
		goto sleep_forever;
	}

	/* Initialize settings subsystem early. */
#ifdef CONFIG_SETTINGS
	ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("Settings subsystem error: %d", ret);
		goto sleep_forever;
	}
#endif

	/* Initialize ESP Environmental Sensor role. */
	ret = bme68x_esp_sensor_init(cb_gap_state_changed);
	if (ret) {
		LOG_ERR("ESP initialization failed: %d", ret);
		goto sleep_forever;
	}

	/* Initialize and configure BSEC IAQ. */
	ret = bme68x_iaq_init();
	if (ret) {
		LOG_ERR("IAQ initialization failed: %d", ret);
		goto sleep_forever;
	}

	/* Run BSEC algorithm control loop. */
	bme68x_iaq_run(&bme68x_dev, iaq_output_handler);

sleep_forever:
	k_sleep(K_FOREVER);
	return 0;
}

void iaq_output_handler(struct bme68x_iaq_sample const *iaq_sample)
{
	static char const *accuracy2str[] = {
		[BME68X_IAQ_ACCURACY_UNRELIABLE] = "unreliable",
		[BME68X_IAQ_ACCURACY_LOW] = "low accuracy",
		[BME68X_IAQ_ACCURACY_MEDIUM] = "medium accuracy",
		[BME68X_IAQ_ACCURACY_HIGH] = "high accuracy",
	};
	static char const *stab2str[] = {
		[BME68X_IAQ_STAB_ONGOING] = "on-going",
		[BME68X_IAQ_STAB_FINISHED] = "finished",
	};

	LOG_INF("T: %.02f degC", (double)iaq_sample->temperature);
	LOG_INF("P: %.01f Pascal", (double)iaq_sample->raw_pressure);
	LOG_INF("H: %.02f %%", (double)iaq_sample->humidity);
	LOG_INF("IAQ: %.02f (%s)", (double)iaq_sample->iaq, accuracy2str[iaq_sample->iaq_accuracy]);
	LOG_INF("stabilization: %s, %s", stab2str[iaq_sample->stab_status],
		stab2str[iaq_sample->run_status]);

	(void)bme68x_ess_update_temperature(iaq_sample->temperature * 100);
	(void)bme68x_ess_update_pressure(iaq_sample->raw_pressure * 10);
	(void)bme68x_ess_update_humidity(iaq_sample->humidity * 100);
}

void cb_gap_state_changed(uint32_t flags, uint8_t conn_avail)
{
	/*
	 * 0x00000001: ADV_AUTO
	 * 0x00010000: ADV_CONN
	 * 0x00020000: CONNECTED
	 * 0x01000000: ENETDOWN
	 */
	LOG_INF("state: 0x%08x (%hu)", flags, conn_avail);
}
