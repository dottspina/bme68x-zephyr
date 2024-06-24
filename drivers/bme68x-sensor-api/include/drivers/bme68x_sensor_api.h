/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>

#include "bme68x_defs.h"

#ifndef DRIVERS_BME68X_SENSOR_API_H_
#define DRIVERS_BME68X_SENSOR_API_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bind BME68X Sensor API communication interface to compatible device.
 *
 * Sets up the platform-specific callbacks on which the BME68X Sensor API
 * implementation eventually relies, in particular the expected I2C/SPI communication interface.
 *
 * This does not communicate with the BME680/688: as usual with the BME68X Sensor API,
 * it is up to the application to actually initialize the sensor with bme68x_init().
 *
 * @param dev A Zephyr device with "bosch,bme68x-sensor-api" bindings.
 * @param bme68x_dev A BME68X Sensor API sensor.
 */
int bme68x_sensor_api_init(struct device const *dev, struct bme68x_dev *bme68x_dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_BME68X_SENSOR_API_H_ */
