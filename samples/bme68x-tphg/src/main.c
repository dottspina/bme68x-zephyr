/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Forced mode TPHG measurements with a single heater set-point.
 *
 * Test application for the BME68X Sensor API driver.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/bme68x_sensor_api.h>

#include "bme68x.h"

#include "bme68x_tphg.h"

LOG_MODULE_REGISTER(app, CONFIG_BME68X_SAMPLE_LOG_LEVEL);

/*
 * User thread definition when testing User Mode.
 */
#if CONFIG_USERSPACE
#define BME68X_TPHG_STACK_SIZE 1024
#define BME68X_TPHG_PRIORITY   (CONFIG_NUM_PREEMPT_PRIORITIES - 1)

K_THREAD_STACK_DEFINE(bme68x_tphg_stack_area, BME68X_TPHG_STACK_SIZE);
struct k_thread bme68x_tphg_thrd;

#endif /* CONFIG_USERSPACE */

/*
 * What to do upon new TPHG measurements.
 */
static void bme68x_tphg_data_sink(struct bme68x_tphg_meas const *meas)
{
#if BME68X_SENSOR_API_FLOAT
	if (meas->gas_valid && meas->heatr_stab) {
		LOG_INF("T:%.02f deg C, P:%.03f kPa, H:%.03f %%, G:%.03f kOhm",
			(double)meas->data.temperature, (double)meas->data.pressure / 1000.0,
			(double)meas->data.humidity, (double)meas->data.gas_resistance / 1000.0);
	} else {
		LOG_INF("T:%.02f deg C, P:%.03f kPa, H:%.03f %%, G:? (0x%0x)",
			(double)meas->data.temperature, (double)meas->data.pressure / 1000.0,
			(double)meas->data.humidity, meas->data.status);
	}
#else
	if (meas->gas_valid && meas->heatr_stab) {
		LOG_INF("T:%d.%02u deg C, P:%u.%03u kPa, H:%u.%03u %%, G:%u.%03u kOhm",
			meas->data.temperature / 100, meas->data.temperature % 100,
			meas->data.pressure / 1000, meas->data.pressure % 1000,
			meas->data.humidity / 1000, meas->data.humidity % 1000,
			meas->data.gas_resistance / 1000, meas->data.gas_resistance % 1000);
	} else {
		LOG_INF("T:%d.%02u deg C, P:%u.%03u kPa, H:%u.%03u %%, G:? (0x%0x)",
			meas->data.temperature / 100, meas->data.temperature % 100,
			meas->data.pressure / 1000, meas->data.pressure % 1000,
			meas->data.humidity / 1000, meas->data.humidity % 1000, meas->data.status);
	}
#endif
}

/*
 * Actual application implementation with prototype compatible
 * with thread entry points.
 *
 * Permits to easily test User Mode.
 */
static void bme68x_tphg_main(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Any compatible device will be fine. */
	struct device const *const dev = DEVICE_DT_GET_ONE(bosch_bme68x_sensor_api);

	struct bme68x_tphg_sensor sensor = {0};
	struct bme68x_tphg_meas tphg_meas = {0};
	uint32_t tphg_cycle_us;

	if (k_is_user_context()) {
		LOG_INF("User mode");
	} else {
		LOG_INF("Supervisor mode");
	}

	int err = bme68x_sensor_api_init(dev, &sensor.dev);
	if (!err) {
		err = bme68x_init(&sensor.dev);
	}
	if (err) {
		LOG_ERR("sensor initialization error: %d", err);
		return;
	}

	err = bme68x_tphg_init(&sensor);
	if (err) {
		LOG_ERR("sensor configuration error: %d", err);
		return;
	}

	/* Should not change unless the sensor is reconfigured. */
	tphg_cycle_us = bme68x_tphg_get_cycle_us(&sensor);
	LOG_INF("TPHG cycle: %u us", tphg_cycle_us);

	for (;;) {
		err = bme68x_tphg_meas_trigger(&sensor, &tphg_cycle_us);
		if (!err) {
			k_sleep(K_USEC(tphg_cycle_us));

			err = bme68x_tphg_meas_read(&sensor, &tphg_meas);
			if (!err && tphg_meas.new_data) {
				bme68x_tphg_data_sink(&tphg_meas);
			}
		}

		if (err) {
			if (err < 0) {
				/*
				 * Negative BME68X Sensor API status indicate fatal errors.
				 */
				LOG_ERR("BME68X Sensor API: %d", err);
				break;

			} else {
				/*
				 * Positive BME68X Sensor API status indicate warnings
				 * that we interpret as "try again/later"
				 */
				LOG_WRN("BME68X Sensor API: %d", err);
			}
		}

		k_sleep(K_SECONDS(BME68X_TPHG_SAMPLE_RATE));
	}
}

int main(void)
{
#if CONFIG_USERSPACE
	/*
	 * Verify that everything still works from user threads.
	 */
	k_thread_create(&bme68x_tphg_thrd, bme68x_tphg_stack_area,
			K_THREAD_STACK_SIZEOF(bme68x_tphg_stack_area), bme68x_tphg_main, NULL, NULL,
			NULL, BME68X_TPHG_PRIORITY, K_USER, K_NO_WAIT);
	k_thread_join(&bme68x_tphg_thrd, K_FOREVER);

#else
	/* Normal supervisor mode. */
	bme68x_tphg_main(NULL, NULL, NULL);
#endif

	return 0;
}
