/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BME68X_ESS_H_
#define BME68X_ESS_H_

/** @brief Environmental Sensing Service (ESS).
 *
 * ESS GATT server.
 *
 * Currently supported ESS characteristics:
 * - Temperature (0x2A6E): GATT Specification Supplement 3.218
 * - Pressure (0x2A6D): GATT Specification Supplement 3.181
 * - Humidity (0x2A6F): GATT Specification Supplement 3.124
 *
 * Each ESS characteristic associates a read-only ES Trigger Setting
 * configured at build-time (Kconfig).
 * All ES Trigger Setting conditions are supported.
 *
 * See Bluetooth SIG Assigned Numbers §6.1.1 for the complete list
 * of defined ESS characteristics.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief ES Trigger Setting Descriptor condition.
 *
 * Environmental Sensing Service §3.1.2.2, Table 3.11
 */
enum es_trigger_setting_condition {
	/** @brief Trigger inactive.
	 *
	 * Peers will be notified depending on their Client Characteristic Configuration,
	 * without any additional condition.
	 */
	ES_TRIGGER_INACTIVE = 0x00,
	/** @brief Use a fixed time interval between transmissions. */
	ES_TRIGGER_FIXED_TIME = 0x01,
	/** @brief No less than the specified time between transmissions. */
	ES_TRIGGER_GTE_TIME = 0x02,
	/** @brief When value changes compared to previous value. */
	ES_TRIGGER_VALUE_CHANGED = 0x03,
	/** @brief While less than the specified value. */
	ES_TRIGGER_LT_VALUE = 0x04,
	/** @brief While less than or equal to the specified value. */
	ES_TRIGGER_LTE_VALUE = 0x05,
	/** @brief While greater than the specified value. */
	ES_TRIGGER_GT_VALUE = 0x06,
	/** @brief While greater than or equal to the specified value. */
	ES_TRIGGER_GTE_VALUE = 0x07,
	/** @brief While equal to the specified value. */
	ES_TRIGGER_SPECIFIED_VALUE = 0x08,
	/** @brief While not equal to the specified value. */
	ES_TRIGGER_NOT_SPECIFIED_VALUE = 0x09,
};

/** @brief ES Trigger Setting Descriptor operand.
 *
 * Environmental Sensing Service §3.1.2.2, Table 3.11
 */
union es_trigger_setting_operand {
	/** @brief Specified time for time-based conditions.
	 *
	 * Unit is seconds.
	 * Type: uint24, M = 1, d = 0, b = 0
	 */
	uint32_t seconds;

	/** @brief Specified value for value-based conditions (sint16, e.g. temperature).
	 */
	int16_t val_sint16;

	/** @brief Specified value for value-based conditions (uint32, e.g. pressure).
	 */
	uint32_t val_uint32;

	/** @brief Specified value for value-based conditions (uint16, e.g. humidity).
	 */
	uint16_t val_uint16;
};

/** @brief ES Trigger Setting Descriptor.
 *
 * Environmental Sensing Service §3.1.2.2
 *
 * NOTE: Trigger settings are read-only, configured at build-time.
 */
struct es_trigger_setting {
	enum es_trigger_setting_condition condition;
	union es_trigger_setting_operand operand;
};

/** @brief Initialize Environmental Sensing Service.
 *
 * Configure ES Trigger Setting descriptors.
 *
 * @return 0 on success, `-EINVAL` on configuration error.
 */
int bme68x_ess_init(void);

/** @brief Update the Temperature characteristic value.
 *
 * Update value in GATT server, notify peers when appropriate.
 *
 * Unit is degrees Celsius with a resolution of 0.01 degrees Celsius.
 * Allowed range: [-273.15, 327.67]
 *
 * @return 0 on success, `-EINVAL` on invalid values.
 */
int bme68x_ess_update_temperature(int16_t temperature);

/** @brief Update the Pressure characteristic value.
 *
 * Update value in GATT server, notify peers when appropriate.
 *
 * Unit is Pascal with a resolution of 0.1 Pascal.
 *
 * @return 0 on success, `-EINVAL` on invalid values.
 */
int bme68x_ess_update_pressure(uint32_t pressure);

/** @brief Update the Humidity characteristic value.
 *
 * Update value in GATT server, notify peers when appropriate.
 *
 * Unit is degrees Percent with a resolution of 0.01 Percent.
 * Allowed range: [0, 10000]
 *
 * @return 0 on success, `-EINVAL` on invalid values.
 */
int bme68x_ess_update_humidity(uint16_t humidity);

#ifdef __cplusplus
}
#endif

#endif // BME68X_ESS_H_
