/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Forced mode TPHG measurements:
 * - with a single heater set-point
 * - LP/ULP sample rates
 */

#ifndef _BME68X_TPHG_H_
#define _BME68X_TPHG_H_

#include "bme68x_defs.h"

/**
 * @brief Initial value of the expected ambient temperature
 * used to compute heater resistance.
 */
#define BME68X_TPHG_AMBIENT_TEMP INT8_C(CONFIG_BME68X_TPHG_AMBIENT_TEMP)

/**
 * @brief TPHG measurement period in seconds.
 */
#define BME68X_TPHG_SAMPLE_RATE UINT32_C(CONFIG_BME68X_TPHG_SAMPLE_RATE)

/**
 * @brief Temperature oversampling.
 */
#if defined(CONFIG_BME68X_TPHG_TEMP_OS_1X)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_1X
#elif defined(CONFIG_BME68X_TPHG_TEMP_OS_2X)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_2X
#elif defined(CONFIG_BME68X_TPHG_TEMP_OS_4X)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_4X
#elif defined(CONFIG_BME68X_TPHG_TEMP_OS_8X)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_8X
#elif defined(CONFIG_BME68X_TPHG_TEMP_OS_16X)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_16X
#elif defined(CONFIG_BME68X_TPHG_TEMP_OS_NONE)
#define BME68X_TPHG_OSX_TEMP BME68X_OS_NONE
#endif

/**
 * @brief Pressure oversampling.
 */
#if defined(CONFIG_BME68X_TPHG_PRESS_OS_1X)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_1X
#elif defined(CONFIG_BME68X_TPHG_PRESS_OS_2X)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_2X
#elif defined(CONFIG_BME68X_TPHG_PRESS_OS_4X)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_4X
#elif defined(CONFIG_BME68X_TPHG_PRESS_OS_8X)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_8X
#elif defined(CONFIG_BME68X_TPHG_PRESS_OS_16X)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_16X
#elif defined(CONFIG_BME68X_TPHG_PRESS_OS_NONE)
#define BME68X_TPHG_OSX_PRESS BME68X_OS_NONE
#endif

/**
 * @brief Humidity oversampling.
 */
#if defined(CONFIG_BME68X_TPHG_HUM_OS_1X)
#define BME68X_TPHG_OSX_HUM BME68X_OS_1X
#elif defined(CONFIG_BME68X_TPHG_HUM_OS_2X)
#define BME68X_TPHG_OSX_HUM BME68X_OS_2X
#elif defined(CONFIG_BME68X_TPHG_HUM_OS_4X)
#define BME68X_TPHG_OSX_HUM BME68X_OS_4X
#elif defined(CONFIG_BME68X_TPHG_HUM_OS_8X)
#define BME68X_TPHG_OSX_HUM BME68X_OS_8X
#elif defined(CONFIG_BME68X_TPHG_HUM_OS_16X)
#define BME68X_TPHG_OSX_HUM BME68X_OS_16X
#elif defined(CONFIG_BME68X_TPHG_HUM_OS_NONE)
#define BME68X_TPHG_OSX_HUM BME68X_OS_NONE
#endif

/**
 * @brief IIR filter coefficient.
 */
#if defined(CONFIG_BME68X_TPHG_FILTER_2)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_1
#elif defined(CONFIG_BME68X_TPHG_FILTER_4)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_3
#elif defined(CONFIG_BME68X_TPHG_FILTER_8)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_7
#elif defined(CONFIG_BME68X_TPHG_FILTER_16)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_15
#elif defined(CONFIG_BME68X_TPHG_FILTER_32)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_31
#elif defined(CONFIG_BME68X_TPHG_FILTER_64)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_63
#elif defined(CONFIG_BME68X_TPHG_FILTER_128)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_SIZE_127
#elif defined(CONFIG_BME68X_TPHG_FILTER_OFF)
#define BME68X_TPHG_IIR_FILTER BME68X_FILTER_OFF
#endif

/**
 * @brief Temperature set-point in degree Celsius.
 */
#define BME68X_TPHG_HEATR_TEMP UINT16_C(CONFIG_BME68X_TPHG_HEATR_TEMP)

/**
 * @brief Heating duration in milliseconds.
 */
#define BME68X_TPHG_HEATR_DUR UINT16_C(CONFIG_BME68X_TPHG_HEATR_DUR)

/**
 * @brief Whether gas measurements are enabled.
 */
#if defined(BME68X_TPHG_HEATR_NONE)
#define BME68X_TPHG_GAS_ENABLE BME68X_DISABLE
#else
#define BME68X_TPHG_GAS_ENABLE BME68X_ENABLE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TPHG measurement.
 */
struct bme68x_tphg_meas {
	/**
	 * @brief Convenience for `new_data_0`.
	 */
	uint8_t new_data;
	/**
	 * @brief Convenience for `gas_valid_r`.
	 */
	uint8_t gas_valid;
	/**
	 * @brief Convenience for `heat_stab_r`.
	 */
	uint8_t heatr_stab;
	/**
	 * @brief Measurement data (as floating point numbers if `CONFIG_BME68X_API_FLOAT`).
	 */
	struct bme68x_data data;
};

/**
 * @brief BME680/688 sensor.
 *
 * Convenience for a BME68X Sensor API sensor and its configuration.
 */
struct bme68x_tphg_sensor {
	/**
	 * @brief BME68X Sensor API sensor.
	 */
	struct bme68x_dev dev;
	/**
	 * @brief Temperature, pressure and humidity sensors configuration.
	 *
	 * Only oversampling and IIR filters are used (no ODR).
	 */
	struct bme68x_conf tph_conf;
	/**
	 * @brief Gas sensor configuration.
	 *
	 * A single heating profile is defined (temperature and duration).
	 */
	struct bme68x_heatr_conf gas_conf;
};

/**
 * @brief Initialize and configure sensor for TPHG measurements.
 *
 * See Kconfig for sensor configuration.
 *
 * @param sensor The sensor to configure.
 *
 * @returns 0 on success, BME68X API return code.
 */
int8_t bme68x_tphg_init(struct bme68x_tphg_sensor *sensor);

/**
 * @brief Configure the temperature, pressure and humidity sensors of the BME680/688.
 *
 * @param sensor The sensor to configure.
 * @param os_temp Temperature oversampling.
 * Zero will switch off temperature measurements (not recommended).
 * @param os_pres Pressure oversampling.
 * Zero will switch off pressure measurements.
 * @param os_temp Relative humidity oversampling.
 * Zero will switch off relative humidity measurements.
 * @param iir_filter IIR filter coefficient. Zero will disable IIR.
 *
 * @returns 0 on success, BME68X API return code.
 */
int8_t bme68x_tphg_configure_tph(struct bme68x_tphg_sensor *sensor, uint8_t os_temp,
				 uint8_t os_pres, uint8_t os_hum, uint8_t iir_filter);

/**
 * @brief Configure the gas resistance sensor of the BME680/688.
 *
 * This configures a single heater set-point.
 *
 * @param sensor The sensor to configure.
 * @param heatr_temp The target temperature in degree Celsius.
 * @param heatr_dur The heating duration in milliseconds.
 * @param gas_enable Whether to actually enable gas measurements (`BME68X_ENABLE`
 * or `BME68X_DISABLE`).
 *
 * @returns 0 on success, BME68X API return code.
 */
int8_t bme68x_tphg_configure_gas(struct bme68x_tphg_sensor *sensor, uint16_t heatr_temp,
				 uint16_t heatr_dur, uint8_t gas_enable);

/**
 * @brief Initiate a TPHG measurement cycle by switching the BME680/688 device to forced mode.
 *
 * The device will return to sleep mode once the measurement is completed.
 *
 * @param sensor The sensor to switch to forced mode.
 * @param cycle_us Output parameter for the TPHG measurement duration in microsecond.
 * This is the time to wait before reading the measure data.
 *
 * @returns 0 on success, BME68X API return code.
 */
int8_t bme68x_tphg_meas_trigger(struct bme68x_tphg_sensor *sensor, uint32_t *cycle_us);

/**
 * @brief Read TPHG data from BME680/688 device registers.
 *
 * @param sensor The sensor from which to read the data registers.
 * @param meas Output parameter for TPHG data.
 *
 * @returns 0 on success, BME68X API return code.
 */
int8_t bme68x_tphg_meas_read(struct bme68x_tphg_sensor *sensor, struct bme68x_tphg_meas *meas);

/**
 * @brief Compute forced mode TPHG measurement cycle duration in microseconds.
 *
 * The cycle includes:
 * - the wake-up time needed to reach the forced mode
 * - the time needed to measure temperature, pressure and humidity
 * - the heating duration needed before we can measure the gas resistance
 *
 * @note We'd prefer a const-qualified sensor, but bme68x_get_meas_dur() expects
 * a non-const device.
 *
 * @param sensor The sensor to compute the TPHG cycle of.
 *
 * @return TPHG cycle duration in microsecond for the current sensor configuration.
 */
uint32_t bme68x_tphg_get_cycle_us(struct bme68x_tphg_sensor *sensor);

#ifdef __cplusplus
}
#endif

#endif /* _BME68X_TPHG_H_ */
