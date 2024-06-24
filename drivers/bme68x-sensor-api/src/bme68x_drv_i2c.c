/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * I2C IO for BME68X Sensor API devices.
 */

#include <zephyr/logging/log.h>

#include "bme68x_drv.h"

LOG_MODULE_DECLARE(bme68x_sensor_api, CONFIG_BME68X_SENSOR_API_DRIVER_LOG_LEVEL);

#if BME68X_DRV_BUS_I2C

/*
 * Implements bme68x_drv_io_check_fn.
 */
static int bme68x_drv_io_check_i2c(union bme68x_drv_bus const *bus)
{
	return i2c_is_ready_dt(&bus->i2c) ? 0 : -ENODEV;
}

/*
 * Implements bme68x_drv_io_write_fn.
 */
static int bme68x_drv_io_write_i2c(struct device const *dev, uint8_t start, uint8_t const *buf,
				   uint32_t len)
{
	struct bme68x_drv_config const *config = dev->config;

	/*
	 * Writing is done by sending pairs of control bytes and register data.
	 */

	/* buf[0] is the data byte for the 1st register. */
	int err = i2c_reg_write_byte_dt(&config->bus.i2c, start, buf[0]);
	if (!err) {
		/* Starting from buf[1], addresses and data interleave. */
		err = i2c_write_dt(&config->bus.i2c, &buf[1], len - 1);
	}

	if (err < 0) {
		LOG_ERR("I2C-write(0x%0x, %u bytes): %d", start, len, err);
	} else {
		LOG_DBG("I2C-write(0x%0x, %u bytes)", start, len);
	}

	return err;
}

/*
 * Implements bme68x_drv_io_read_fn.
 */
static int bme68x_drv_io_read_i2c(struct device const *dev, uint8_t start, uint8_t *buf,
				  uint32_t len)
{
	struct bme68x_drv_config const *config = dev->config;

	/*
	 * BME680/688 devices support multiple byte read (using a single register address
	 * which is auto-incremented), we can read several continuous registers
	 * with a single I2C control byte.
	 */
	int err = i2c_burst_read_dt(&config->bus.i2c, start, buf, len);

	if (err < 0) {
		LOG_ERR("I2C-read(0x%0x, %u bytes): %d", start, len, err);
	} else {
		LOG_DBG("I2C-read(0x%0x, %u bytes)", start, len);
	}
	return err;
}

struct bme68x_drv_io const bme68x_drv_io_i2c = {
	.check = bme68x_drv_io_check_i2c,
	.read = bme68x_drv_io_read_i2c,
	.write = bme68x_drv_io_write_i2c,
};

#endif /* BME68X_DRV_BUS_I2C */
