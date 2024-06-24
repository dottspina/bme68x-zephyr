/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This driver is intended for use with the Bosch Sensortec BME68X Sensor API.
 * Compatible devices don't provide the Zephyr Sensors API.
 *
 * This is a private header, public API is in include/drivers/bme68x-sensor-api.h.
 */

#ifndef _BME68X_DRV_H_
#define _BME68X_DRV_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/types.h>

#include "bme68x_defs.h"

/* Driver for "bosch,bme68x-sensor-api" bindings. */
#define DT_DRV_COMPAT bosch_bme68x_sensor_api

/* Whether SPI support is required by a compatible device. */
#define BME68X_DRV_BUS_SPI DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)
/* Whether I2C support is required by a compatible device. */
#define BME68X_DRV_BUS_I2C DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)

/* Per-instance bus specification (I2C/SPI). */
union bme68x_drv_bus {
#if BME68X_DRV_BUS_SPI
	struct spi_dt_spec spi;
#endif
#if BME68X_DRV_BUS_I2C
	struct i2c_dt_spec i2c;
#endif
};

/*
 * Check bus instance (ala device_is_ready()).
 * Returns 0 on success, -ENODEV on error.
 */
typedef int (*bme68x_drv_io_check_fn)(union bme68x_drv_bus const *bus);

/*
 * Device driver IO operation for reading BME680/688 registers.
 *
 * This operation is always called through bme68x_get_regs(),
 * which therefore specifies the semantic of the parameters:
 *
 * start: Address of the first BME680/688 register to read:
 *        - I2C: 8bit register address
 *        - SPI: reg_addr | BME68X_SPI_RD_MSK = reg_addr | 0x80
 *          Note that bme68x_get_regs() also sets the SPI memory page as needed
 *          before the actual read transaction.
 *
 * buf: Destination buffer.
 * len: Number of bytes (registers) to read.
 *
 * Returns 0 on success, -EIO on error.
 */
typedef int (*bme68x_drv_io_read_fn)(struct device const *dev, uint8_t start, uint8_t *buf,
				     uint32_t len);

/*
 * Device driver IO operation for writing BME680/688 registers.
 *
 * This operation is always called through bme68x_set_regs(),
 * which therefore specifies the semantic of the parameters:
 *
 * start: Address of the first BME680/688 register to write:
 *        - I2C: 8bit register address
 *        - SPI: reg_addr & BME68X_SPI_WR_MSK = reg_addr & 0x7f
 *          Note that bme68x_set_regs() also sets the SPI memory page as needed
 *          before the actual write transaction.
 *
 * buf: Source buffer, starts with the data byte of the first register,
 *      then interleaves addresses and data
 *
 * len: Size of the source buffer,
 *      such that the number of registers we'll write is 1 + (len - 1) / 2
 *
 * Returns 0 on success, -EIO on error.
 */
typedef int (*bme68x_drv_io_write_fn)(struct device const *dev, uint8_t start, uint8_t const *buf,
				      uint32_t len);

/*
 * Device driver IO operations.
 *
 * One instance per required bus protocol (I2C/SPI).
 */
struct bme68x_drv_io {
	bme68x_drv_io_check_fn check;
	bme68x_drv_io_read_fn read;
	bme68x_drv_io_write_fn write;
};

#if BME68X_DRV_BUS_SPI
#define BME68X_DRV_SPI_OPERATION                                                                   \
	(SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER)
/* See bme68x_drv_spi.c */
extern struct bme68x_drv_io const bme68x_drv_io_spi;
#endif

#if BME68X_DRV_BUS_I2C
/* See bme68x_drv_i2c.c */
extern struct bme68x_drv_io const bme68x_drv_io_i2c;
#endif

/* Driver instance configuration (private immutable). */
struct bme68x_drv_config {
	/* Bus the BME680/688 device is connected to. */
	union bme68x_drv_bus bus;
	/* IO operations appropriate for the above bus type. */
	struct bme68x_drv_io const *bus_io;
};

/*
 * Private system call for BME68X Sensor API callback bme68x_read_fptr_t.
 *
 * intf_ptr: Device with "bosch,bme68x-sensor-api" bindings.
 *
 * Semantic of other parameters is specified by bme68x_drv_io_read_fn.
 *
 * Returns BME68X_OK on success, BME68X_E_COM_FAIL on error.
 */
__syscall BME68X_INTF_RET_TYPE bme68x_sensor_api_read(uint8_t start, uint8_t *buf, uint32_t len,
						      void *intf_ptr);

/*
 * Private system call for BME68X Sensor API callback bme68x_write_fptr_t.
 *
 * intf_ptr: Device with "bosch,bme68x-sensor-api" bindings.
 *
 * Semantic of other parameters is specified by bme68x_drv_io_write_fn.
 *
 * Returns BME68X_OK on success, BME68X_E_COM_FAIL on error.
 */
__syscall BME68X_INTF_RET_TYPE bme68x_sensor_api_write(uint8_t start, uint8_t const *buf,
						       uint32_t len, void *intf_ptr);

/*
 * Private system call for checking device bus (ala device_is_ready()).
 *
 * dev: "bosch,bme68x-sensor-api" compatible device.
 *
 * Returns 0 on success, -ENODEV on error.
 */
__syscall int bme68x_sensor_api_check(struct device const *dev);

#include "syscalls/bme68x_drv.h"
#endif /* _BME68X_DRV_H_ */
