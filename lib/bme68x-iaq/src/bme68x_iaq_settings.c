/*
 * Copyright (c) 2025, Christophe Dufaza
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_iaq_settings.h"

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <stdint.h>

LOG_MODULE_DECLARE(bme68x_iaq, CONFIG_BME68X_IAQ_LOG_LEVEL);

#define BME68X_IAQ_SETTINGS_SUBTREE    "bme68x-iaq"
#define BME68X_IAQ_SETTINGS_BSEC_STATE "bsec/state"

struct iaq_settings_handle {
	/* Buffer, must be capable of holding BSEC_MAX_STATE_BLOB_SIZE bytes. */
	uint8_t *bsec_state;
	/* Once successfully loaded, size of the BSEC state retrieved from settings. */
	size_t bsec_state_len;
};

static int cb_iaq_settings_load(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
				void *param);

int bme68x_iaq_settings_read_bsec_state(uint8_t *data, uint32_t *len)
{
	/* Assuming BSEC_MAX_STATE_BLOB_SIZE bytes allocated for bsec_state data. */
	struct iaq_settings_handle handle = {.bsec_state = data, .bsec_state_len = 0};

	int err = settings_load_subtree_direct(BME68X_IAQ_SETTINGS_SUBTREE, cb_iaq_settings_load,
					       &handle);

	if (err) {
		LOG_ERR("failed to load settings: %d", err);
		return err;
	}

	if (handle.bsec_state_len == 0) {
		LOG_DBG("no saved settings available");
		return -ENOENT;
	}

	*len = handle.bsec_state_len;
	return 0;
}

int bme68x_iaq_settings_write_bsec_state(uint8_t const *data, uint32_t len)
{
	int err = settings_save_one(BME68X_IAQ_SETTINGS_SUBTREE "/" BME68X_IAQ_SETTINGS_BSEC_STATE,
				    data, len);
	if (err) {
		LOG_ERR("failed to save settings for BSEC state: %d", err);
	}
	return err;
}

int bme68x_iaq_settings_delete_bsec_state(void)
{
	int err = settings_delete(BME68X_IAQ_SETTINGS_SUBTREE "/" BME68X_IAQ_SETTINGS_BSEC_STATE);
	if (err) {
		LOG_ERR("failed to delete settings for BSEC state: %d", err);
	}
	return err;
}

int cb_iaq_settings_load(char const *key, size_t len, settings_read_cb read_cb, void *cb_arg,
			 void *param)
{
	LOG_DBG("settings key: %s, %d bytes", key, len);

	char const *next;
	if (settings_name_steq(BME68X_IAQ_SETTINGS_BSEC_STATE, key, &next) && !next) {
		/* Exact match BME68X_IAQ_SETTINGS_SUBTREE/BME68X_IAQ_SETTINGS_BSEC_STATE */
		struct iaq_settings_handle *handle = param;

		ssize_t rc = read_cb(cb_arg, handle->bsec_state, len);
		if (rc < 0) {
			LOG_ERR("failed to read settings for BSEC state: %d", rc);
			/*
			 * Note: When a non zero value is returned,
			 * further subtree searching is stopped.
			 */
			return rc;
		}

		/*
		 * A length of zero (deleted key) will also be consistent
		 * with the "no saved state available" semantic.
		 */
		handle->bsec_state_len = rc;
	}

	/* Ignore unexpected keys. */
	return 0;
}
