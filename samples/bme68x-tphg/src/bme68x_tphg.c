/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_tphg.h"

#include <zephyr/logging/log.h>

#include "bme68x.h"

LOG_MODULE_DECLARE(app, CONFIG_BME68X_SAMPLE_LOG_LEVEL);

/* Get strings from configuration values. */
static inline char const *tph_conf_osx2str(uint8_t osx);
static inline char const *tph_conf_iir2str(uint8_t filter);

/*
 * Configure BME680/688 sensor for TPHG measurements (with settings from Kconfig).
 * @returns 0 on success, BME68X API return code.
 */
static int8_t bme68x_tphg_configure(struct bme68x_tphg_sensor *sensor);

int8_t bme68x_tphg_init(struct bme68x_tphg_sensor *sensor)
{
	int8_t ret = bme68x_init(&sensor->dev);

	if (ret == BME68X_OK) {
		sensor->dev.amb_temp = BME68X_TPHG_AMBIENT_TEMP;

		ret = bme68x_tphg_configure(sensor);
	}
	return ret;
}

int8_t bme68x_tphg_configure_tph(struct bme68x_tphg_sensor *sensor, uint8_t os_temp,
				 uint8_t os_pres, uint8_t os_hum, uint8_t iir_filter)
{
	struct bme68x_conf conf = {
		.os_temp = os_temp,
		.os_pres = os_pres,
		.os_hum = os_hum,
		.filter = iir_filter,
		.odr = BME68X_ODR_NONE,
	};
	int8_t ret = bme68x_set_conf(&conf, &sensor->dev);

	if (ret == BME68X_OK) {
		sensor->tph_conf = conf;

		LOG_INF("os_t:%s os_p:%s os_h:%s iir:%s",
			tph_conf_osx2str(sensor->tph_conf.os_temp),
			tph_conf_osx2str(sensor->tph_conf.os_pres),
			tph_conf_osx2str(sensor->tph_conf.os_hum),
			tph_conf_iir2str(sensor->tph_conf.filter));
	} else {
		LOG_ERR("sensor configuration error: %d", ret);
	}
	return ret;
}

int8_t bme68x_tphg_configure_gas(struct bme68x_tphg_sensor *sensor, uint16_t heatr_temp,
				 uint16_t heatr_dur, uint8_t gas_enable)
{
	struct bme68x_heatr_conf heatr_conf = {
		.heatr_temp = heatr_temp,
		.heatr_dur = heatr_dur,
		.enable = gas_enable,
	};
	int8_t ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &sensor->dev);

	if (ret == BME68X_OK) {
		sensor->gas_conf = heatr_conf;

		LOG_INF("heatr_temp:%d degC  heatr_dur:%u ms", sensor->gas_conf.heatr_temp,
			sensor->gas_conf.heatr_dur);
	} else {
		LOG_ERR("heating profile error: %d", ret);
	}
	return ret;
}

int8_t bme68x_tphg_meas_trigger(struct bme68x_tphg_sensor *sensor, uint32_t *cycle_us)
{
	int8_t ret = bme68x_set_op_mode(BME68X_FORCED_MODE, &sensor->dev);

	if (ret == BME68X_OK) {
		*cycle_us = bme68x_tphg_get_cycle_us(sensor);

	} else {
		LOG_ERR("failed to switch to forced: %d", ret);
	}
	return ret;
}

int8_t bme68x_tphg_meas_read(struct bme68x_tphg_sensor *sensor, struct bme68x_tphg_meas *meas)
{
	int8_t ret;
	uint8_t n_data; /* Always 0 or 1 in forced mode.  */

	ret = bme68x_get_data(BME68X_FORCED_MODE, &meas->data, &n_data, &sensor->dev);
	if (ret == BME68X_OK) {
		meas->new_data = meas->data.status & BME68X_NEW_DATA_MSK;
		meas->heatr_stab = meas->data.status & BME68X_HEAT_STAB_MSK;
		meas->gas_valid = meas->data.status & BME68X_GASM_VALID_MSK;

	} else {
		LOG_ERR("failed to read BME68X data: %d", ret);
	}

	return ret;
}

uint32_t bme68x_tphg_get_cycle_us(struct bme68x_tphg_sensor *sensor)
{
	uint32_t heatr_dur_us = sensor->gas_conf.heatr_dur * UINT32_C(1000);
	uint32_t meas_dur_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &sensor->tph_conf,
						   &sensor->dev);
	return meas_dur_us + heatr_dur_us;
}

int8_t bme68x_tphg_configure(struct bme68x_tphg_sensor *sensor)
{
	int8_t ret = bme68x_tphg_configure_tph(sensor, BME68X_TPHG_OSX_TEMP, BME68X_TPHG_OSX_PRESS,
					       BME68X_TPHG_OSX_HUM, BME68X_TPHG_IIR_FILTER);

	if (ret == BME68X_OK) {
		ret = bme68x_tphg_configure_gas(sensor, BME68X_TPHG_HEATR_TEMP,
						BME68X_TPHG_HEATR_DUR, BME68X_TPHG_GAS_ENABLE);
	}

	return ret;
}

char const *tph_conf_osx2str(uint8_t osx)
{
	switch (osx) {
	case BME68X_OS_1X:
		return "x1";
	case BME68X_OS_2X:
		return "x2";
	case BME68X_OS_4X:
		return "x4";
	case BME68X_OS_8X:
		return "x8";
	case BME68X_OS_16X:
		return "x16";
	case BME68X_OS_NONE:
		return "off";
	default:
		return "?";
	}
}
char const *tph_conf_iir2str(uint8_t filter)
{
	switch (filter) {
	case BME68X_FILTER_SIZE_1:
		return "2";
	case BME68X_FILTER_SIZE_3:
		return "4";
	case BME68X_FILTER_SIZE_7:
		return "8";
	case BME68X_FILTER_SIZE_15:
		return "16";
	case BME68X_FILTER_SIZE_31:
		return "32";
	case BME68X_FILTER_SIZE_63:
		return "64";
	case BME68X_FILTER_SIZE_127:
		return "128";
	case BME68X_FILTER_OFF:
		return "off";
	default:
		return "?";
	}
}
