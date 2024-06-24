/*
 * Copyright (c) 2024, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BSEC state persistence to Flash storage with Zephyr NVS.
 */

#ifndef BME68X_IAQ_NVS_H_
#define BME68X_IAQ_NVS_H_

#include <zephyr/kernel.h>

/**
 * @brief Whether BSEC state persistence to Flash storage (NVS) is supported.
 *
 * This does not imply that the application will actually initialize
 * and use this service.
 */
#if defined(CONFIG_BME68X_IAQ_NVS)
#define BME68X_IAQ_NVS_ENABLED 1
#else
#define BME68X_IAQ_NVS_ENABLED 0
#endif

/**
 * @brief Devicetree label of the Flash partition dedicated to this NVS file-system.
 */
#define BME68X_IAQ_NVS_PARTITION_LABEL bsec_partition

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS support.
 *
 * @return 0 on success, negative errno otherwise.
 */
#if BME68X_IAQ_NVS_ENABLED
__syscall int bme68x_iaq_nvs_init(void);
#else
int bme68x_iaq_nvs_init(void);
#endif

/**
 * @brief Read BSEC state from NVS.
 *
 * @param data Destination buffer for BSEC state data.
 * Recommended buffer size if `BSEC_MAX_STATE_BLOB_SIZE` bytes
 * @param len Size in bytes of the BSEC state retrieved from NVS.
 *
 * @return 0 on success, negative errno otherwise (`-ENOENT` if no saved state available).
 */
#if BME68X_IAQ_NVS_ENABLED
__syscall int bme68x_iaq_nvs_read_state(uint8_t *data, uint32_t *len);
#else
int bme68x_iaq_nvs_read_state(uint8_t *data, uint32_t *len);
#endif

/**
 * @brief Write BSEC state to NVS.
 *
 * @param data The buffer that contains the state data.
 * @param len Length of the state data in bytes.
 *
 * @return 0 on success, negative errno otherwise.
 */
#if BME68X_IAQ_NVS_ENABLED
__syscall int bme68x_iaq_nvs_write_state(uint8_t const *data, uint32_t len);
#else
int bme68x_iaq_nvs_write_state(uint8_t const *data, uint32_t len);
#endif

/**
 * @brief Delete saved state from NVS.
 *
 * The flash partition isn't erased until reclaimed.
 *
 * @return 0 on success, negative errno otherwise (`-ENOENT` if no saved state available).
 */
#if BME68X_IAQ_NVS_ENABLED
__syscall int bme68x_iaq_nvs_delete_state(void);
#else
int bme68x_iaq_nvs_delete_state(void);
#endif

#ifdef __cplusplus
}
#endif

#if BME68X_IAQ_NVS_ENABLED
#include "syscalls/bme68x_iaq_nvs.h"
#endif

#endif /* BME68X_IAQ_NVS_H_ */
