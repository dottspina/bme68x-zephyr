/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_drv.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/bme68x_sensor_api.h>

#include "bme68x_defs.h"

/*
 * Within each initialization level you may specify a priority level,
 * relative to other devices in the same initialization level.
 * The priority level is specified as an integer value in the range 0 to 99;
 * lower values indicate earlier initialization.
 *
 * See also: https://github.com/zephyrproject-rtos/zephyr/pull/73836
 */
#define BME68X_SENSOR_API_DRIVER_INIT_PRIORITY CONFIG_BME68X_SENSOR_API_DRIVER_INIT_PRIORITY

LOG_MODULE_REGISTER(bme68x_sensor_api, CONFIG_BME68X_SENSOR_API_DRIVER_LOG_LEVEL);

#if BME68X_DRV_BUS_SPI
static inline bool bme68x_is_on_spi(struct device const *dev)
{
	struct bme68x_drv_config const *config = dev->config;

	return config->bus_io == &bme68x_drv_io_spi;
}
#endif

static inline int bme68x_drv_bus_check(struct device const *dev)
{
	struct bme68x_drv_config const *config = dev->config;

	return config->bus_io->check(&config->bus);
}

/*
 * Provides bme68x_delay_us_fptr_t.
 */
static void bme68x_sensor_api_delay_us(uint32_t period, void *intf_ptr)
{
	k_sleep(K_USEC(period));
}

/*
 * Binds BME68X Sensor API communication interface to compatible device.
 */
int bme68x_sensor_api_init(struct device const *dev, struct bme68x_dev *bme68x_dev)
{
	if (bme68x_sensor_api_check(dev) < 0) {
		return -ENODEV;
	}

	bme68x_dev->intf_ptr = NULL;

#if BME68X_DRV_BUS_SPI
	if (bme68x_is_on_spi(dev)) {
		bme68x_dev->intf = BME68X_SPI_INTF;
		/* intf_ptr is not const-qualified. */
		bme68x_dev->intf_ptr = (void *)dev;
	}
#endif

	if (!bme68x_dev->intf_ptr) {
		bme68x_dev->intf = BME68X_I2C_INTF;
		/* intf_ptr is not const-qualified. */
		bme68x_dev->intf_ptr = (void *)dev;
	}

	bme68x_dev->read = bme68x_sensor_api_read;
	bme68x_dev->write = bme68x_sensor_api_write;
	bme68x_dev->delay_us = bme68x_sensor_api_delay_us;

	LOG_INF("%s (%s API)", dev->name,
		BME68X_SENSOR_API_FLOAT ? "Floating-point" : "Fixed-point");

	return 0;
}

/*
 * Driver instance initialization.
 *
 * Contrary to upstream BME680 driver, this implementation does not actually initialize
 * the sensor (reset, calibration data, etc): it's up to application code
 * to call bme68x_init(), as usual with the BME68X Sensor API.
 */
static int bme68x_drv_init(struct device const *dev)
{
	int err = bme68x_drv_bus_check(dev);
	if (err < 0) {
		LOG_ERR("%s: bus error %d", dev->name, err);
	} else {
		LOG_DBG("new device: %s", dev->name);
	}
	return err;
}

/*
 * Provides bme68x_read_fptr_t as system call.
 */
BME68X_INTF_RET_TYPE z_impl_bme68x_sensor_api_read(uint8_t start, uint8_t *buf, uint32_t length,
						   void *intf_ptr)
{
	struct device const *dev = intf_ptr;
	struct bme68x_drv_config const *config = dev->config;

	int err = config->bus_io->read(dev, start, buf, length);
	if (err < 0) {
		return BME68X_E_COM_FAIL;
	}
	return BME68X_OK;
}

/*
 * Provides bme68x_write_fptr_t as system call.
 */
BME68X_INTF_RET_TYPE z_impl_bme68x_sensor_api_write(uint8_t start, uint8_t const *buf,
						    uint32_t length, void *intf_ptr)
{
	struct device const *dev = intf_ptr;
	struct bme68x_drv_config const *config = dev->config;

	int err = config->bus_io->write(dev, start, buf, length);
	if (err < 0) {
		return BME68X_E_COM_FAIL;
	}
	return BME68X_OK;
}

int z_impl_bme68x_sensor_api_check(struct device const *dev)
{
	return bme68x_drv_bus_check(dev);
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>

BME68X_INTF_RET_TYPE z_vrfy_bme68x_sensor_api_read(uint8_t start, uint8_t *buf, uint32_t length,
						   void *intf_ptr)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(buf, length));
	return z_impl_bme68x_sensor_api_read(start, buf, length, intf_ptr);
}
#include <syscalls/bme68x_sensor_api_read_mrsh.c>

BME68X_INTF_RET_TYPE z_vrfy_bme68x_sensor_api_write(uint8_t start, uint8_t const *buf,
						    uint32_t length, void *intf_ptr)
{
	K_OOPS(K_SYSCALL_MEMORY_READ(buf, length));
	return z_impl_bme68x_sensor_api_write(start, buf, length, intf_ptr);
}
#include <syscalls/bme68x_sensor_api_write_mrsh.c>

int z_vrfy_bme68x_sensor_api_check(struct device const *dev)
{
	return z_impl_bme68x_sensor_api_check(dev);
}
#include <syscalls/bme68x_sensor_api_check_mrsh.c>

#endif /* CONFIG_USERSPACE */

#define BME68X_DRV_CONFIG_SPI(inst)                                                                \
	{                                                                                          \
		.bus.spi = SPI_DT_SPEC_INST_GET(inst, BME68X_DRV_SPI_OPERATION, 0),                \
		.bus_io = &bme68x_drv_io_spi,                                                      \
	}

#define BME68X_DRV_CONFIG_I2C(inst)                                                                \
	{                                                                                          \
		.bus.i2c = I2C_DT_SPEC_INST_GET(inst), .bus_io = &bme68x_drv_io_i2c,               \
	}

#define BME68X_DRV_DEFINE(inst)                                                                    \
	static struct bme68x_drv_config const bme68x_drv_config_##inst = COND_CODE_1(              \
		DT_INST_ON_BUS(inst, spi), (BME68X_DRV_CONFIG_SPI(inst)),                          \
		(BME68X_DRV_CONFIG_I2C(inst)));                                                    \
	DEVICE_DT_INST_DEFINE(inst, bme68x_drv_init, NULL, NULL, &bme68x_drv_config_##inst,        \
			      POST_KERNEL, BME68X_SENSOR_API_DRIVER_INIT_PRIORITY, NULL);

/* Create driver instances for enabled compatible devices. */
DT_INST_FOREACH_STATUS_OKAY(BME68X_DRV_DEFINE)
