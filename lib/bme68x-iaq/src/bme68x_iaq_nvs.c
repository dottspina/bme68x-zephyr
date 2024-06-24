/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * A BSEC state save consists of two NVS elements:
 * - STATE_LEN: size of last saved state (4 bytes, plus 8 bytes of meta-data)
 * - STATE_BLOB: last saved state data (typically 220 bytes, plus 8 bytes of meta-data)
 */

#include "bme68x_iaq_nvs.h"

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

/*
 * NVSFS identifier for the state's length.
 */
#define BME68X_IAQ_NVS_BSEC_STATE_LEN_ID 1U

/*
 * NVSFS identifier for the state's data.
 */
#define BME68X_IAQ_NVS_BSEC_STATE_BLOB_ID 2U

#define BME68X_IAQ_NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(BME68X_IAQ_NVS_PARTITION_LABEL)
#define BME68X_IAQ_NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(BME68X_IAQ_NVS_PARTITION_LABEL)

LOG_MODULE_DECLARE(bme68x_iaq, CONFIG_BME68X_IAQ_LOG_LEVEL);

#if !BME68X_IAQ_NVS_ENABLED
int bme68x_iaq_nvs_init(void)
{
	LOG_WRN("NVS support disabled");
	return -ENOSYS;
}
int bme68x_iaq_nvs_read_state(uint8_t *data, uint32_t *len)
{
	LOG_WRN("NVS support disabled");
	return -ENOSYS;
}
int bme68x_iaq_nvs_write_state(uint8_t const *data, uint32_t len)
{
	LOG_WRN("NVS support disabled");
	return -ENOSYS;
}
int bme68x_iaq_nvs_delete_state(void)
{
	LOG_WRN("NVS support disabled");
	return -ENOSYS;
}
#else

static int write_bsec_state_len(uint32_t len);
static int write_bsec_state_blob(uint8_t const *data, uint32_t len);
static int read_bsec_state_len(uint32_t *len);
static int read_bsec_state_blob(uint8_t *data, uint32_t len);
static int delete_bsec_state_len(void);
static int delete_bsec_state_blob(void);

/* Dedicated file-system instance. */
static struct nvs_fs nvsfs;

int z_impl_bme68x_iaq_nvs_init(void)
{
	nvsfs.flash_device = BME68X_IAQ_NVS_PARTITION_DEVICE;
	if (!device_is_ready(nvsfs.flash_device)) {
		LOG_ERR("flash device not ready: %s", nvsfs.flash_device->name);
		return -ENODEV;
	}

	struct flash_pages_info page_info;
	nvsfs.offset = BME68X_IAQ_NVS_PARTITION_OFFSET;
	int ret = flash_get_page_info_by_offs(nvsfs.flash_device, nvsfs.offset, &page_info);

	if (!ret) {
		/* Set sector size to page size. */
		nvsfs.sector_size = page_info.size;
		/* Minimum number of sectors required by NVS. */
		nvsfs.sector_count = 2;
		ret = nvs_mount(&nvsfs);
	}

	if (!ret) {
		LOG_INF("NVS-FS at 0x%lx (%u x %u bytes)", page_info.start_offset,
			nvsfs.sector_count, nvsfs.sector_size);
	} else {
		LOG_ERR("NVS-FS initialization failed: %d", ret);
	}

	return ret;
}

int z_impl_bme68x_iaq_nvs_read_state(uint8_t *data, uint32_t *len)
{
	int ret = read_bsec_state_len(len);
	if (!ret) {
		ret = read_bsec_state_blob(data, *len);

		if (ret == -ENOENT) {
			/* If we got the length element, we should also get a blob element. */
			LOG_ERR("STATE_BLOB not found");
			ret = -EINVAL;
		}
	}
	return ret;
}

int z_impl_bme68x_iaq_nvs_write_state(uint8_t const *data, uint32_t len)
{
	/*
	 * Write STATE_BLOB first, creating BSEC state data.
	 * Write STATE_LEN only if we successfully wrote the state data.
	 */
	int ret = write_bsec_state_blob(data, len);
	if (!ret) {
		ret = write_bsec_state_len(len);
	}
	return ret;
}

int z_impl_bme68x_iaq_nvs_delete_state(void)
{
	/*
	 * Delete STATE_LEN first to invalidate the saved BSEC state, if any.
	 * Do not delete STATE_BLOB if we failed to invalidate an existing saved state.
	 */
	int ret = delete_bsec_state_len();
	if (!ret || (ret == -ENOENT)) {
		ret = delete_bsec_state_blob();
	}
	return ret;
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>

int z_vrfy_bme68x_iaq_nvs_init(void)
{
	return z_impl_bme68x_iaq_nvs_init();
}
#include <syscalls/bme68x_iaq_nvs_init_mrsh.c>

int z_vrfy_bme68x_iaq_nvs_read_state(uint8_t *data, uint32_t *len)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(data, sizeof(*len)));
	return z_impl_bme68x_iaq_nvs_read_state(data, len);
}
#include <syscalls/bme68x_iaq_nvs_read_state_mrsh.c>

int z_vrfy_bme68x_iaq_nvs_write_state(uint8_t const *data, uint32_t len)
{
	K_OOPS(K_SYSCALL_MEMORY_READ(data, len));
	return z_impl_bme68x_iaq_nvs_write_state(data, len);
}
#include <syscalls/bme68x_iaq_nvs_write_state_mrsh.c>

int z_vrfy_bme68x_iaq_nvs_delete_state(void)
{
	return z_impl_bme68x_iaq_nvs_delete_state();
}
#include <syscalls/bme68x_iaq_nvs_delete_state_mrsh.c>

#endif /* CONFIG_USERSPACE */

int read_bsec_state_len(uint32_t *len)
{
	size_t sz_state_len = sizeof(*len);
	ssize_t ret = nvs_read(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_LEN_ID, len, sz_state_len);

	if (ret == sz_state_len) {
		/* On success, returns the number of bytes requested to be read. */
		return 0;
	}

	if (ret < 0) {
		/* On error, returns negative value of errno.h. */
		if (ret == -ENOENT) {
			LOG_DBG("no STATE_LEN entry");
		} else {
			LOG_ERR("failed to read STATE_LEN: %d", ret);
		}
		return ret;
	}

	/*
	 * Otherwise, the return value is larger than the number of bytes requested to read,
	 * this indicates not all bytes were read, and more data is available.
	 */
	LOG_ERR("invalid STATE_LEN: %u/%u bytes", sz_state_len, ret);
	return -ERANGE;
}

int read_bsec_state_blob(uint8_t *state, uint32_t len)
{
	ssize_t ret = nvs_read(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_BLOB_ID, state, len);

	if (ret == len) {
		return 0;
	}

	if (ret < 0) {
		if (ret == -ENOENT) {
			LOG_DBG("no STATE_BLOB entry");
		} else {
			LOG_ERR("failed to read STATE_BLOB: %d", ret);
		}
		return ret;
	}

	LOG_ERR("invalid STATE_BLOB: %u/%u bytes", len, ret);
	return -ERANGE;
}

int write_bsec_state_len(uint32_t len)
{
	size_t sz_state_len = sizeof(len);
	ssize_t ret = nvs_write(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_LEN_ID, &len, sz_state_len);

	if (ret < 0) {
		/* On error, returns negative value of errno.h defined error codes. */
		LOG_ERR("failed to write STATE_LEN: %d", ret);
		return ret;
	}

	if (!ret) {
		/*
		 * On success, should return the number of bytes requested to be read.
		 * When a rewrite of the same data already stored is attempted,
		 * nothing is written to flash, thus 0 is returned.
		 */
		LOG_DBG("same STATE_LEN data, skipped");
	}
	return 0;
}

int write_bsec_state_blob(uint8_t const *data, uint32_t len)
{
	ssize_t ret = nvs_write(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_BLOB_ID, data, len);

	if (ret < 0) {
		LOG_ERR("failed to write STATE_BLOB: %d", ret);
		return ret;
	}

	if (!ret) {
		LOG_DBG("same STATE_BLOB data, skipped");
	}

	return 0;
}

int delete_bsec_state_len(void)
{
	int ret = nvs_delete(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_LEN_ID);
	if (ret && (ret != -ENOENT)) {
		LOG_ERR("failed to delete STATE_LEN: %d", ret);
	}
	return ret;
}

int delete_bsec_state_blob(void)
{
	int ret = nvs_delete(&nvsfs, BME68X_IAQ_NVS_BSEC_STATE_BLOB_ID);
	if (ret && (ret != -ENOENT)) {
		LOG_ERR("failed to delete STATE_BLOB: %d", ret);
	}
	return ret;
}

#endif /* BME68X_IAQ_NVS_ENABLED */
