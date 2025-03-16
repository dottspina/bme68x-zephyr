/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Support API for Index for Air Quality (IAQ) with Bosch Sensortec Environmental Cluster (BSEC)
 * and the BME68X Sensor API.
 */

#ifndef BME68X_IAQ_H_
#define BME68X_IAQ_H_

#include "bme68x_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** BSEC output signal accuracy.
 *
 * See `bsec_output_t::accuracy`.
 */
enum bme68x_iaq_accuracy {
	/** Sensor data is unreliable, the sensor must be calibrated. */
	BME68X_IAQ_ACCURACY_UNRELIABLE = 0,
	/** Reliability of virtual sensor is low, sensor should be calibrated. */
	BME68X_IAQ_ACCURACY_LOW = 1,
	/** Medium reliability, sensor calibration or training may improve. */
	BME68X_IAQ_ACCURACY_MEDIUM = 2,
	/** High reliability. */
	BME68X_IAQ_ACCURACY_HIGH = 3,
};

/** Stabilization status.
 *
 * See BSEC signals `BSEC_OUTPUT_STABILIZATION_STATUS` and `BSEC_OUTPUT_RUN_IN_STATUS`.
 */
enum bme68x_iaq_status {
	/** Stabilization is ongoing. */
	BME68X_IAQ_STAB_ONGOING = 0,
	/** Stabilization is finished. */
	BME68X_IAQ_STAB_FINISHED = 1,
};

/**
 * @brief IAQ output signals produced by the BSEC algorithm.
 */
struct bme68x_iaq_sample {
	/** Timestamp in ns. */
	int64_t ts_ns;
	/** Number of BSEC output signals updated during the last algorithm iteration. */
	uint8_t cnt_outputs;
	/** Temperature directly measured by BME68x in degree Celsius. */
	float raw_temperature;
	/** Pressure directly measured by the BME68x in Pa. */
	float raw_pressure;
	/** Relative humidity directly measured by the BME68x in %. */
	float raw_humidity;
	/** Gas resistance measured directly by the BME68x in Ohm. */
	float raw_gas_res;
	/** Sensor heat compensated temperature in degrees Celsius. */
	float temperature;
	/** Sensor heat compensated relative humidity in %. */
	float humidity;
	/** Indoor-air-quality estimate, [0-500] (from clean to heavily polluted air). */
	float iaq;
	/** IAQ estimate accuracy. */
	enum bme68x_iaq_accuracy iaq_accuracy;
	/**
	 * @brief CO2 equivalent estimate in ppm.
	 *
	 * - less than 350 ppm: normal outdoor
	 * - less than 1000: normal indoor
	 * - more than 2000: headaches, nausea, etc
	 * - more than 5000: unusual conditions, risk of toxicity
	 * - more than 40000: immediate danger
	 * - 250000: death
	 */
	float co2_equivalent;
	/** CO2 estimate accuracy. */
	enum bme68x_iaq_accuracy co2_accuracy;
	/**
	 * @brief VOC estimate in ppm.
	 *
	 * WHO standards for total VOC (TVOC):
	 * - target level under 0.05 ppm or 0.25 mg/m3
	 * - VOC ppm from 0.20 to 0.61 is only okay for temporary exposure
	 * - anything over 0.61 ppm is considered a dangerous TVOC level
	 */
	float voc_equivalent;
	/** VOC estimate accuracy. */
	enum bme68x_iaq_accuracy voc_accuracy;
	/** Percentage of min and max filtered gas value. */
	float gas_percentage;
	/** Gas sensor stabilization status. */
	enum bme68x_iaq_status stab_status;
	/** Gas sensor run-in status. */
	enum bme68x_iaq_status run_status;
};

/**
 * @brief Synchronous callback for handling the IAQ output samples produced by the BSEC algorithm.
 *
 * Callbacks are assumed to consume the samples: the memory location
 * of the iaq_sample parameter is invalid once the handler has returned.
 */
typedef void (*bme68x_iaq_output_cb)(struct bme68x_iaq_sample const *iaq_sample);

/**
 * @brief Initialize and configure the BSEC algorithm.
 *
 * - initialize BSEC library
 * - load the selected IAQ configuration (Kconfig)
 * - if BSEC state persistence is enabled (Kconfig),
 *   initialize NVS file-system and load saved BSEC state
 * - subscribe to all virtual sensors supported in IAQ mode
 *
 * @return 0 on success.
 */
int bme68x_iaq_init(void);

/**
 * @brief Run BSEC algorithm control loop.
 *
 * Put BME68X sensor under control of the BSEC algorithm to produce IAQ estimates.
 *
 * This function won't return unless a fatal error occurs.
 *
 * @param dev The controlled BME68X sensor.
 * @param iaq_output_handler Synchronous callback that will consume the produced IAQ outputs.
 */
void bme68x_iaq_run(struct bme68x_dev *dev, bme68x_iaq_output_cb iaq_output_handler);

#ifdef __cplusplus
}
#endif

#endif /* BME68X_IAQ_H_ */
