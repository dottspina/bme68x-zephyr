/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_iaq.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bme68x.h"
#include "bsec_interface.h"

/*
 * Selected (Kconfig) BSEC algorithm configuration for IAQ.
 *
 * Defines configuration blob as bsec_config_iaq.
 *
 * NOTE: Typical configuration is just over 2 kB.
 */
#include "bsec_iaq.h"

/*
 * Sample rate of the BSEC virtual sensor:
 * - LP: 1/3 Hz
 * - ULP: 1/300 Hz
 */
#if defined(CONFIG_BME68X_IAQ_SAMPLE_RATE_LP)
#define BME68X_IAQ_SAMPLE_RATE BSEC_SAMPLE_RATE_LP
#elif defined(CONFIG_BME68X_IAQ_SAMPLE_RATE_ULP)
#define BME68X_IAQ_SAMPLE_RATE BSEC_SAMPLE_RATE_ULP
#endif

/*
 * Initial temperature used to compute heater resistance,
 * aka expected temperature, in degree Celsius.
 */
#define BME68X_IAQ_AMBIENT_TEMP INT8_C(CONFIG_BME68X_IAQ_AMBIENT_TEMP)

#ifdef CONFIG_BME68X_IAQ_SETTINGS
/* BSEC state persistence to per-device settings. */
#include "bme68x_iaq_settings.h"

/* BSEC state saves periodicity in minutes, zero means disabled.
 * Dismiss any negative interval from invalid configuration.
 */
static int iaq_state_saves_intvl = CONFIG_BME68X_IAQ_STATE_SAVE_INTVL > 0
					   ? CONFIG_BME68X_IAQ_STATE_SAVE_INTVL
					   : 0;

/* Should we delete any previously saved BSEC state on reset ? */
#ifdef CONFIG_BME68X_IAQ_RST_SAVED_STATE
#define IAQ_RST_SAVED_STATE 1
#else
#define IAQ_RST_SAVED_STATE 0
#endif

/*
 * Retrieve BSEC state from saved settings, if available.
 *
 * Returns 0 on success, -ENOENT if no saved state available,
 * other non zero values on error.
 */
static int iaq_bsec_load_state(void);

/*
 * BSEC state persistence to per-device settings:
 * - save settings for BSEC state
 * - restart timer on success, disable periodic saves on error
 *
 * Called periodically by the IAQ control loop.
 */
static void iaq_bsec_save_state(void);

/* Delete BSEC state from settings. */
static void iaq_bsec_delete_state(void);

/*
 * Dedicated timer:
 * - started just before entering the BSEC control loop
 * - then managed by iaq_bsec_save_state()
 */
K_TIMER_DEFINE(iaq_state_save_timer, NULL, NULL);

#endif /* CONFIG_BME68X_IAQ_SETTINGS */

/*
 *  Configure the BSEC algorithm for IAQ.
 *
 * Configuration options (Kconfig):
 * - Sample rate (LP or ULP)
 * - Supply voltage (1.8 V or 3.3 V)
 * - Calibration time (4 or 28 days)
 *
 * These options together identify a BSEC configuration blob,
 * e.g. `bme680_iaq_33v_3s_4d`.
 *
 * Returns 0 on success, BSEC status otherwise.
 */
static bsec_library_return_t iaq_bsec_configure(void);

/*
 * Subscribe to all virtual sensors supported in IAQ mode.
 *
 * Returns 0 on success, BSEC status otherwise.
 */
static bsec_library_return_t iaq_bsec_subscribe(void);

/*
 * Trigger the TPHG measurements requested by the BSEC control loop:
 * - configure BME68X sensor with requested settings
 * - switch the sensor to forced mode, triggering the measurement cycle
 *
 * sensor_settings: BSEC control request
 * dev: the controlled BME68X sensor
 *
 * Returns 0 on success, a BME68X Sensor API status otherwise.
 */
static int8_t iaq_bsec_trigger_measurement(bsec_bme_settings_t const *sensor_settings,
					   struct bme68x_dev *dev);

/*
 * Populate BSEC inputs with TPHG data.
 *
 * sensor_settings: BSEC control request
 * ts_ns: timestamp of the BSEC control loop iteration
 * bme68x_data: TPHG data from controlled BME68X device
 * bsec_inputs: BSEC algorithm inputs to configure
 *
 * Returns the number of configured BSEC inputs (typically 4, TPHG).
 */
static size_t iaq_bsec_set_inputs(bsec_bme_settings_t const *sensor_settings, int64_t ts_ns,
				  struct bme68x_data const *bme68x_data,
				  bsec_input_t bsec_inputs[BSEC_MAX_PHYSICAL_SENSOR]);
/*
 * Process TPHG measurement triggered by the BSEC algorithm:
 * - retrieve the data from the controlled BME68X sensor registers
 * - configure BSEC inputs with the new data
 * - run BSEC algorithm to process inputs into IAQ output signals
 *
 * sensor_settings: BSEC control request
 * ts_ns: timestamp of the BSEC control loop iteration
 * dev: controlled BME68X sensor
 * iaq_sample: output parameter, IAQ output signals
 *
 * Returns 0 on success, BSEC or BME68X Sensor API status otherwise.
 */
static int iaq_next_sample(bsec_bme_settings_t const *sensor_settings, int64_t ts_ns,
			   struct bme68x_dev *dev, struct bme68x_iaq_sample *iaq_sample);

/*
 * Compute forced mode TPHG measurement duration in microseconds.
 *
 * The duration includes:
 * - the wake-up time needed to reach the forced mode
 * - the time needed to measure temperature, pressure, and humidity
 * - the heating duration needed before we can measure the gas resistance
 *
 * sensor_settings: BSEC control request
 *
 * Returns the TPHG cycle duration in microseconds.
 */
static uint32_t iaq_get_tphg_meas_dur(bsec_bme_settings_t const *sensor_settings);

/*
 * Uptime in 64-bit nanosecond precision:
 * - timestamps for the BSEC algorithm iterations
 * - compute rendez-vous with BSEC control
 *
 * NOTE: For a system clock of frequency 32768 Hz,
 * this uptime won't overflow and remain monotonic for about 584 years.
 *
 * See:
 * - Kernel Timing API
 * - CONFIG_SYS_CLOCK_MAX_TIMEOUT_DAYS
 *
 * Returns the system up-time in nanoseconds.
 */
static int64_t iaq_uptime_ns(void);

/*
 * Populate IAQ sample with BSEC output signals.
 *
 * ts_ns: timestamp of the BSEC control loop iteration
 * bsec_outputs: BSEC output signals
 * n_output: number of BSEC output signals
 * iaq_sample: IAQ sample to populate
 */
static void iaq_sample_set_outputs(int64_t ts_ns, bsec_output_t const *bsec_outputs,
				   size_t n_output, struct bme68x_iaq_sample *iaq_output);

/*
 * Virtual sensors for all BSEC outputs supported in IAQ mode.
 */
static bsec_sensor_configuration_t const iaq_virt_sensors[] = {
	{
		.sensor_id = BSEC_OUTPUT_RAW_TEMPERATURE,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_RAW_PRESSURE,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_RAW_HUMIDITY,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_RAW_GAS,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_IAQ,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_GAS_PERCENTAGE,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_RUN_IN_STATUS,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
	{
		.sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS,
		.sample_rate = BME68X_IAQ_SAMPLE_RATE,
	},
};

LOG_MODULE_REGISTER(bme68x_iaq, CONFIG_BME68X_IAQ_LOG_LEVEL);

int bme68x_iaq_init(void)
{
	bsec_version_t ver;
	int ret = bsec_get_version(&ver);
	if (!ret) {
		ret = bsec_init();
	}
	if (ret) {
		LOG_ERR("BSEC library unavailable: %d", ret);
		return ret;
	}
	LOG_INF("BSEC %hu.%hu.%hu.%hu", ver.major, ver.minor, ver.major_bugfix, ver.minor_bugfix);

	ret = iaq_bsec_configure();
	if (ret) {
		return ret;
	}

#ifdef CONFIG_BME68X_IAQ_SETTINGS
	/* On startup, try to either reset or load any previously saved BSEC state,
	 * according to configuration. */
	if (IAQ_RST_SAVED_STATE) {
		iaq_bsec_delete_state();
	} else {
		ret = iaq_bsec_load_state();

		if (ret && (ret != -ENOENT)) {
			return ret;
		}
	}
#endif /* CONFIG_BME68X_IAQ_SETTINGS */

	ret = iaq_bsec_subscribe();
	return ret;
}

void bme68x_iaq_run(struct bme68x_dev *dev, bme68x_iaq_output_cb iaq_output_handler)
{
	bsec_bme_settings_t sensor_settings = {0};

	/* Initialize temperature used to compute heater resistance. */
	dev->amb_temp = BME68X_IAQ_AMBIENT_TEMP;

#ifdef CONFIG_BME68X_IAQ_SETTINGS
	/* Enable periodic BSEC state persistence. */
	if (iaq_state_saves_intvl > 0) {
		k_timer_start(&iaq_state_save_timer, K_MINUTES(iaq_state_saves_intvl), K_NO_WAIT);

		LOG_INF("BSEC state save period: %d min", iaq_state_saves_intvl);
	} else {
		LOG_INF("BSEC state periodic saves disabled");
	}
#endif /* CONFIG_BME68X_IAQ_SETTINGS */

	/*
	 * Run the algorithm until negative BME68X Sensor API/BSEC status code.
	 */
	for (int ret = 0; ret >= 0;) {
		ret = 0;

		int64_t ts_ns = iaq_uptime_ns();
		if (ts_ns < sensor_settings.next_call) {
			/*
			 * Too early at BSEC control rendez-vous,
			 * continue until it's time to get the next BSEC request.
			 */
			continue;
		}

		sensor_settings = (bsec_bme_settings_t){0};
		ret = bsec_sensor_control(ts_ns, &sensor_settings);
		if (ret) {
			if (ret < 0) {
				LOG_ERR("BSEC control error: %d", ret);
			} else {
				/*
				 * Typically, we're too late (BSEC_W_SC_CALL_TIMING_VIOLATION)
				 * because the difference between two consecutive measurements
				 * is greater than allowed.
				 * For example, in LP mode, sampling rate 3 seconds,
				 * the difference between two measurements (algorithm iterations)
				 * must no exceed 106.25% of 3 s, which is 3.1875 s.
				 *
				 * TPHG wait: 239590 us
				 * BSEC wait: 2747772 us
				 * IAQ loop total wait: 2987362 us
				 * IAQ loop body: 3187500 - 2987362 = 200138 us
				 *
				 * We'll then be too late if running the BSEC algorithm
				 * iteration and the IAQ output handler,
				 * plus the needed I2C/SPI communications,
				 * exceeds 200 ms.
				 */
				LOG_WRN("BSEC control status: %d", ret);
			}
			goto iaq_loop_next;
		}

		if (!sensor_settings.trigger_measurement) {
			/* Nothing to do. */
			goto iaq_loop_next;
		}

		ret = iaq_bsec_trigger_measurement(&sensor_settings, dev);
		if (ret) {
			goto iaq_loop_next;
		}

		uint32_t tphg_us = iaq_get_tphg_meas_dur(&sensor_settings);
		LOG_DBG("TPHG wait: %u us ...", tphg_us);
		k_sleep(K_USEC(tphg_us));

		struct bme68x_iaq_sample iaq_sample;
		ret = iaq_next_sample(&sensor_settings, ts_ns, dev, &iaq_sample);
		if (ret) {
			goto iaq_loop_next;
		}

		if (iaq_sample.cnt_outputs) {
			iaq_output_handler(&iaq_sample);

			/* Update temperature used to compute heater resistance. */
			dev->amb_temp = (int8_t)iaq_sample.temperature;
		}

#ifdef CONFIG_BME68X_IAQ_SETTINGS
		if ((iaq_state_saves_intvl > 0) && !k_timer_remaining_get(&iaq_state_save_timer)) {
			/* Save state to settings, reset timer on success,
			 * disable periodic saves on error. */
			iaq_bsec_save_state();
		}
#endif /* CONFIG_BME68X_IAQ_SETTINGS */

	iaq_loop_next:
		/* Non recoverable error, exit IAQ loop immediately. */
		if (ret < 0) {
			continue;
		}

		int64_t next_rdv_ns = (sensor_settings.next_call - iaq_uptime_ns());
		LOG_DBG("BSEC wait: %lld us ...", next_rdv_ns / 1000);
		k_sleep(K_NSEC(next_rdv_ns));
	}

#ifdef CONFIG_BME68X_IAQ_SETTINGS
	k_timer_stop(&iaq_state_save_timer);
#endif
}

bsec_library_return_t iaq_bsec_configure(void)
{
	/* NOTE: stack size > 4096 bytes. */
	uint8_t buf[BSEC_MAX_WORKBUFFER_SIZE];
	size_t const sz_conf = sizeof(bsec_config_iaq);
	bsec_library_return_t ret = bsec_set_configuration(bsec_config_iaq, sz_conf, buf,
							   sizeof(buf));

	if (ret) {
		LOG_ERR("BSEC configuration failed: %d", ret);
	} else {
		LOG_INF("loaded BSEC configuration (%u bytes)", sz_conf);
	}
	return ret;
}

bsec_library_return_t iaq_bsec_subscribe(void)
{
	uint8_t n_phy = BSEC_MAX_PHYSICAL_SENSOR;
	bsec_sensor_configuration_t phy_sensors[BSEC_MAX_PHYSICAL_SENSOR];
	size_t const n_subscriptions = ARRAY_SIZE(iaq_virt_sensors);

	bsec_library_return_t ret = bsec_update_subscription(iaq_virt_sensors, n_subscriptions,
							     phy_sensors, &n_phy);

	if (ret) {
		LOG_ERR("BSEC subscriptions failed: %d", ret);
	} else {
		LOG_INF("BSEC subscriptions: %u/%u", n_subscriptions, n_phy);
	}
	return ret;
}

int8_t iaq_bsec_trigger_measurement(bsec_bme_settings_t const *sensor_settings,
				    struct bme68x_dev *dev)
{
	struct bme68x_conf conf = {
		.os_temp = sensor_settings->temperature_oversampling,
		.os_pres = sensor_settings->pressure_oversampling,
		.os_hum = sensor_settings->humidity_oversampling,
		.odr = BME68X_ODR_NONE,
	};
	struct bme68x_heatr_conf heatr_conf = {
		.enable = sensor_settings->run_gas,
		.heatr_temp = sensor_settings->heater_temperature,
		.heatr_dur = sensor_settings->heater_duration,
	};

	int8_t ret = bme68x_set_conf(&conf, dev);
	if (ret) {
		LOG_ERR("oversampling configuration failed: %d", ret);
		return ret;
	}
	LOG_DBG("os_t:%hu os_p:%hu os_h:%hu", conf.os_temp, conf.os_pres, conf.os_hum);

	ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, dev);
	if (ret) {
		LOG_ERR("heater configuration failed: %d", ret);
		return ret;
	}
	LOG_DBG("heatr_temp(degC):%u heatr_dur(ms):%u", heatr_conf.heatr_temp,
		heatr_conf.heatr_dur);

	ret = bme68x_set_op_mode(BME68X_FORCED_MODE, dev);
	if (ret) {
		LOG_ERR("switching sensor to forced mode failed: %d", ret);
	} else {
		LOG_DBG("forced mode");
	}

	return ret;
}

size_t iaq_bsec_set_inputs(bsec_bme_settings_t const *sensor_settings, int64_t ts_ns,
			   struct bme68x_data const *bme68x_data,
			   bsec_input_t bsec_inputs[BSEC_MAX_PHYSICAL_SENSOR])
{
	size_t n_inputs = 0;

	if (sensor_settings->process_data & BSEC_PROCESS_TEMPERATURE) {
		bsec_inputs[n_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
		bsec_inputs[n_inputs].time_stamp = ts_ns;
		if (BME68X_SENSOR_API_FLOAT) {
			/* Temperature from BME68X API is floating-point degC. */
			bsec_inputs[n_inputs].signal = bme68x_data->temperature;
		} else {
			/*
			 * Temperature from BME68X API is an integer
			 * scaled to centidegrees (x100 degC).
			 */
			bsec_inputs[n_inputs].signal = bme68x_data->temperature / 100.0f;
		}
		n_inputs++;
	}

	if (sensor_settings->process_data & BSEC_PROCESS_HUMIDITY) {
		bsec_inputs[n_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
		bsec_inputs[n_inputs].time_stamp = ts_ns;
		if (BME68X_SENSOR_API_FLOAT) {
			/*
			 * Relative humidity from BME68X API is floating-point percentage.
			 */
			bsec_inputs[n_inputs].signal = bme68x_data->humidity;
		} else {
			/* Relative humidity from BME68X API is an integer
			 * scaled to millipercent (x1000).
			 */
			bsec_inputs[n_inputs].signal = bme68x_data->humidity / 1000.0f;
		}
		n_inputs++;
	}

	if (sensor_settings->process_data & BSEC_PROCESS_PRESSURE) {
		bsec_inputs[n_inputs].sensor_id = BSEC_INPUT_PRESSURE;
		bsec_inputs[n_inputs].time_stamp = ts_ns;
		/*
		 * Pressure from BME68X API in Pascal,
		 * either floating-point or fixed point.
		 */
		bsec_inputs[n_inputs].signal = bme68x_data->pressure;
		n_inputs++;
	}

	if (sensor_settings->process_data & BSEC_PROCESS_GAS) {
		/* NOTE: Should we skip this BSEC input when not BME68X_GASM_VALID_MSK ? */
		bsec_inputs[n_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
		bsec_inputs[n_inputs].time_stamp = ts_ns;
		/*
		 * Gas resistance from BME68X API in Ohm,
		 * either floating-point or fixed point.
		 */
		bsec_inputs[n_inputs].signal = bme68x_data->gas_resistance;
		n_inputs++;
	}

	return n_inputs;
}

int iaq_next_sample(bsec_bme_settings_t const *sensor_settings, int64_t ts_ns,
		    struct bme68x_dev *dev, struct bme68x_iaq_sample *iaq_sample)
{
	uint8_t n_data; /* Ignored, always 1 on success in IAQ mode. */
	struct bme68x_data bme68x_data;
	int ret = bme68x_get_data(sensor_settings->op_mode, &bme68x_data, &n_data, dev);
	if (ret) {
		if (ret < 0) {
			LOG_ERR("failed to read BME68X data: %d", ret);
		} else {
			LOG_DBG("no new data: %d", ret);
		}
		return ret;
	}

	bsec_input_t bsec_inputs[BSEC_MAX_PHYSICAL_SENSOR];
	bsec_output_t bsec_outputs[ARRAY_SIZE(iaq_virt_sensors)];
	uint8_t n_outputs = ARRAY_SIZE(bsec_outputs);

	uint8_t n_inputs = iaq_bsec_set_inputs(sensor_settings, ts_ns, &bme68x_data, bsec_inputs);

	ret = bsec_do_steps(bsec_inputs, n_inputs, bsec_outputs, &n_outputs);
	if (ret) {
		if (ret < 0) {
			LOG_ERR("BSEC algorithm error: %d", ret);
		} else {
			LOG_WRN("BSEC algorithm status: %d", ret);
		}
		return ret;
	}

	iaq_sample_set_outputs(ts_ns, bsec_outputs, n_outputs, iaq_sample);
	return 0;
}

uint32_t iaq_get_tphg_meas_dur(bsec_bme_settings_t const *sensor_settings)
{
	static uint8_t const os_to_meas_cycles[6] = {0, 1, 2, 4, 8, 16};

	/*
	 * TPH measurement duration (us), implementation borrowed
	 * from bme68x_get_meas_dur(): this permits to not involve the device here,
	 * and keep API const-qualified.
	 */
	uint32_t meas_cycles = os_to_meas_cycles[sensor_settings->temperature_oversampling];
	meas_cycles += os_to_meas_cycles[sensor_settings->pressure_oversampling];
	meas_cycles += os_to_meas_cycles[sensor_settings->humidity_oversampling];
	uint32_t meas_dur = meas_cycles * UINT32_C(1963);
	meas_dur += UINT32_C(477 * 4); /* TPH switching duration */
	meas_dur += UINT32_C(477 * 5); /* Gas measurement duration */
	meas_dur += UINT32_C(1000);    /* Wake up duration of 1ms */

	/* Add the time needed to reach the heater set-point. */
	uint32_t heatr_dur = sensor_settings->heater_duration * 1000U;

	return meas_dur + heatr_dur;
}

void iaq_sample_set_outputs(int64_t ts_ns, bsec_output_t const *bsec_outputs, size_t n_outputs,
			    struct bme68x_iaq_sample *iaq_sample)
{
	iaq_sample->ts_ns = ts_ns;
	iaq_sample->cnt_outputs = n_outputs;

	for (size_t i = 0; i < n_outputs; i++) {
		switch (bsec_outputs[i].sensor_id) {
		case BSEC_OUTPUT_RAW_TEMPERATURE:
			iaq_sample->raw_temperature = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_RAW_PRESSURE:
			iaq_sample->raw_pressure = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_RAW_HUMIDITY:
			iaq_sample->raw_humidity = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_RAW_GAS:
			iaq_sample->raw_gas_res = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
			iaq_sample->temperature = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
			iaq_sample->humidity = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_IAQ:
			iaq_sample->iaq = bsec_outputs[i].signal;
			iaq_sample->iaq_accuracy =
				(enum bme68x_iaq_accuracy)bsec_outputs[i].accuracy;
			break;
		case BSEC_OUTPUT_CO2_EQUIVALENT:
			iaq_sample->co2_equivalent = bsec_outputs[i].signal;
			iaq_sample->co2_accuracy =
				(enum bme68x_iaq_accuracy)bsec_outputs[i].accuracy;
			break;
		case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
			iaq_sample->voc_equivalent = bsec_outputs[i].signal;
			iaq_sample->voc_accuracy =
				(enum bme68x_iaq_accuracy)bsec_outputs[i].accuracy;
			break;
		case BSEC_OUTPUT_GAS_PERCENTAGE:
			iaq_sample->gas_percentage = bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_STABILIZATION_STATUS:
			iaq_sample->stab_status = (enum bme68x_iaq_status)bsec_outputs[i].signal;
			break;
		case BSEC_OUTPUT_RUN_IN_STATUS:
			iaq_sample->run_status = (enum bme68x_iaq_status)bsec_outputs[i].signal;
			break;
		default:
			break;
		}
	}
}

int64_t iaq_uptime_ns(void)
{
	int64_t ticks = k_uptime_ticks();
	/*
	 * Unsigned k_ticks_to_ns_floor64(ticks) will overflow before we loose one bit
	 * for the sign, and likely after 584 years.
	 */
	return (int64_t)k_ticks_to_ns_floor64(ticks);
}

#if CONFIG_BME68X_IAQ_SETTINGS
int iaq_bsec_load_state(void)
{
	/* NOTE: stack size > 221 + 4086 (4307 bytes). */
	uint8_t data[BSEC_MAX_STATE_BLOB_SIZE];
	uint8_t buf[BSEC_MAX_WORKBUFFER_SIZE];
	uint32_t len;

	int ret = bme68x_iaq_settings_read_bsec_state(data, &len);
	if (ret) {
		if (ret == -ENOENT) {
			LOG_INF("no saved BSEC state available");
		} else {
			LOG_ERR("failed to read BSEC state: %d", ret);
		}
		return ret;
	}

	ret = bsec_set_state(data, len, buf, BSEC_MAX_WORKBUFFER_SIZE);
	if (ret) {
		LOG_ERR("failed to set BSEC state: %d", ret);
	} else {
		LOG_INF("loaded BSEC state (%u bytes)", len);
	}

	return ret;
}

void iaq_bsec_save_state(void)
{
	/* NOTE: stack size > 221 + 4086 (4307 bytes). */
	uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
	uint8_t buf[BSEC_MAX_WORKBUFFER_SIZE];
	uint32_t len;

	int ret = bsec_get_state(0, state, sizeof(state), buf, sizeof(buf), &len);
	if (ret) {
		LOG_ERR("BSEC state unavailable: %d", ret);
		return;
	}

	ret = bme68x_iaq_settings_write_bsec_state(state, len);

	if (ret) {
		k_timer_stop(&iaq_state_save_timer);
		iaq_state_saves_intvl = 0;

		LOG_ERR("failed to save BSEC state: %d", ret);
		LOG_ERR("BSEC state persistence disabled");

	} else {
		LOG_INF("saved BSEC state (%u bytes)", len);
		k_timer_start(&iaq_state_save_timer, K_MINUTES(iaq_state_saves_intvl), K_NO_WAIT);
	}
}

void iaq_bsec_delete_state(void)
{
	int err = bme68x_iaq_settings_delete_bsec_state();
	if (err) {
		LOG_ERR("failed to delete BSEC state: %d", err);
	} else {
		LOG_INF("deleted BSEC state");
	}
}
#endif /* CONFIG_BME68X_IAQ_SETTINGS */
