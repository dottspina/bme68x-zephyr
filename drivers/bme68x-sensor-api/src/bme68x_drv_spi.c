/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPI IO for BME68X Sensor API devices.
 */

#include <zephyr/logging/log.h>

#include "bme68x_drv.h"

LOG_MODULE_DECLARE(bme68x_sensor_api, CONFIG_BME68X_SENSOR_API_DRIVER_LOG_LEVEL);

#if BME68X_DRV_BUS_SPI

/*
 * Implements bme68x_drv_io_check_fn.
 */
static int bme68x_drv_io_check_spi(union bme68x_drv_bus const *bus)
{
	return spi_is_ready_dt(&bus->spi) ? 0 : -ENODEV;
}

/*
 * Implements bme68x_drv_io_write_fn.
 *
 * NOTE: This is called by the BME68X Sensor API which already takes care of setting:
 * - the SPI memory page appropriate for the BME68X register address
 * - the RW bit to 0 for an SPI write command
 */
static int bme68x_drv_io_write_spi(struct device const *dev, uint8_t start, uint8_t const *buf,
				   uint32_t len)
{
	struct bme68x_drv_config const *config = dev->config;

	uint8_t buf_reg0[] = {start, buf[0]};
	struct spi_buf const tx_bufs[] = {
		/* buf[0] is the data byte for the 1st register. */
		{.buf = &buf_reg0, .len = ARRAY_SIZE(buf_reg0)},
		/* Starting from buf[1], addresses and data interleave. */
		{.buf = (void *)&buf[1], .len = len - 1},
	};

	struct spi_buf_set const tx = {.buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs)};

	int err = spi_write_dt(&config->bus.spi, &tx);

	if (err < 0) {
		LOG_ERR("SPI-write(0x%0x, %u): %d", start, len, err);
	} else {
		LOG_DBG("SPI-write(0x%0x, %u bytes)", start, len);
	}
	return err;
}

/*
 * Implements bme68x_drv_io_read_fn.
 *
 * NOTE: The BME68X Sensor API already takes care of setting:
 * - the SPI memory page appropriate for the BME68X register address
 * - the RW bit to 1 for an SPI read command
 */
static int bme68x_drv_io_read_spi(struct device const *dev, uint8_t start, uint8_t *buf,
				  uint32_t len)
{
	struct bme68x_drv_config const *config = dev->config;

	/*
	 * SPI read: the BME680/688 register address is automatically incremented,
	 * we can read several continuous registers without sending new SPI control bytes.
	 */

	struct spi_buf const tx_buf = {.buf = &start, .len = 1};
	struct spi_buf_set const tx = {.buffers = &tx_buf, .count = 1};

	struct spi_buf rx_buf[2] = {{.buf = NULL, .len = 1}, {.buf = buf, .len = len}};
	struct spi_buf_set const rx = {.buffers = rx_buf, .count = ARRAY_SIZE(rx_buf)};

	int err = spi_transceive_dt(&config->bus.spi, &tx, &rx);

	if (err < 0) {
		LOG_ERR("SPI read(0x%0x, %u bytes): %d", start, len, err);
	} else {
		LOG_DBG("SPI-read(0x%0x, %u bytes)", start, len);
	}
	return err;
}

struct bme68x_drv_io const bme68x_drv_io_spi = {
	.check = bme68x_drv_io_check_spi,
	.read = bme68x_drv_io_read_spi,
	.write = bme68x_drv_io_write_spi,
};
#endif /* BME68X_DRV_BUS_SPI */
