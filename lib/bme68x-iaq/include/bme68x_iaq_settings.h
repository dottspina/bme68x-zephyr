/*
 * Copyright (c) 2025, Christophe Dufaza
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** BSEC state persistence to per-device settings (Zephyr Settings subsystem).
 */

#ifndef BME68X_IAQ_SETTINGS_H_
#define BME68X_IAQ_SETTINGS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieve BSEC state from saved settings.
 *
 * The Settings subsystem must have previously been initialized.
 *
 * @param data On success, will hold the BSEC state data.
 *             The size of the provided buffer should match the largest
 *             BSEC state data (BSEC_MAX_STATE_BLOB_SIZE, 221 bytes).
 * @param len On success, will hold the size of the retrieved BSEC state.
 *
 * @return 0 on success, -ENOENT if no saved settings available,
 *           other non zero values on error.
 */
int bme68x_iaq_settings_read_bsec_state(uint8_t *data, uint32_t *len);

/**
 * @brief Save BSEC state to settings.
 *
 * The Settings subsystem must have previously been initialized.
 *
 * @param data Buffer of BSEC state data.
 * @param len Size of the BSEC state data.
 *
 * @return 0 on success, non zero values on error.
 */
int bme68x_iaq_settings_write_bsec_state(uint8_t const *data, uint32_t len);

/**
 * @brief Delete BSEC state saved to settings, if any.
 *
 * @return 0 on success, non zero values on error.
 */
int bme68x_iaq_settings_delete_bsec_state(void);

#ifdef __cplusplus
}
#endif

#endif // BME68X_IAQ_SETTINGS_H_
