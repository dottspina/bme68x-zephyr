/*
 * Copyright (c) 2025, Chris Duf
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme68x_ess.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(esp_ess, CONFIG_BME68X_ESP_LOG_LEVEL);

/* Placeholder for trigger settings without operand. */
#define TRIGGER_SETTING_OPERAND_NONE 0

/* Temperature ES Trigger Setting configuration. */
#define TEMPERATURE_TRIGGER_CONDITION CONFIG_BME68X_TEMPERATURE_TRIGGER_CONDITION
#ifdef CONFIG_BME68X_TEMPERATURE_TRIGGER_OPERAND
#define TEMPERATURE_TRIGGER_HAS_OPERAND   1
#define TEMPERATURE_TRIGGER_OPERAND_VALUE CONFIG_BME68X_TEMPERATURE_TRIGGER_OPERAND
#else
#define TEMPERATURE_TRIGGER_HAS_OPERAND   0
#define TEMPERATURE_TRIGGER_OPERAND_VALUE TRIGGER_SETTING_OPERAND_NONE
#endif

/* Pressure ES Trigger Setting configuration. */
#define PRESSURE_TRIGGER_CONDITION CONFIG_BME68X_PRESSURE_TRIGGER_CONDITION
#ifdef CONFIG_BME68X_PRESSURE_TRIGGER_OPERAND
#define PRESSURE_TRIGGER_HAS_OPERAND   1
#define PRESSURE_TRIGGER_OPERAND_VALUE CONFIG_BME68X_PRESSURE_TRIGGER_OPERAND
#else
#define PRESSURE_TRIGGER_HAS_OPERAND   0
#define PRESSURE_TRIGGER_OPERAND_VALUE TRIGGER_SETTING_OPERAND_NONE
#endif

/* Humidity ES Trigger Setting configuration. */
#define HUMIDITY_TRIGGER_CONDITION CONFIG_BME68X_HUMIDITY_TRIGGER_CONDITION
#ifdef CONFIG_BME68X_HUMIDITY_TRIGGER_OPERAND
#define HUMIDITY_TRIGGER_HAS_OPERAND   1
#define HUMIDITY_TRIGGER_OPERAND_VALUE CONFIG_BME68X_HUMIDITY_TRIGGER_OPERAND
#else
#define HUMIDITY_TRIGGER_HAS_OPERAND   0
#define HUMIDITY_TRIGGER_OPERAND_VALUE TRIGGER_SETTING_OPERAND_NONE
#endif

#define ESS_GATT_TEMPERATURE_UNKNOWN ((int16_t)0x8000)
#define ESS_GATT_HUMIDITY_UNKNOWN    ((uint16_t)0xffff)

/* ESS characteristic state. */
struct ess_characteristic {

	/* Permits to retrieve the corresponding GATT attribute with bt_gatt_find_by_uuid(). */
	struct bt_uuid const *const uuid;

	/* Attribute value, signedness and size depends on characteristic (up to 32-bit).
	 * Read accesses: caused by GATT read operations (including notifications)
	 * Write accesses: caused by API calls like bme68x_ess_update_temperature()
	 */
	atomic_t value;

	/* Attribute value timestamp used for the ES_TRIGGER_GTE_TIME trigger setting
	 * condition (32-bit unsigned, seconds since system boot).
	 *
	 * Only accessed from sequential API calls like bme68x_ess_update_temperature()
	 */
	uint32_t value_ts;

	/* Whether there's at least one connected peer that has enabled
	 * notifications in its CCC for this characteristic.
	 *
	 * Read accesses: caused by API calls like bme68x_ess_update_temperature()
	 * Write accesses: caused by CCC changed events
	 */
	atomic_t ccc_notify;

	/* Read-only trigger setting for this characteristic. */
	struct es_trigger_setting trigger_setting;

	/* Used when the trigger setting condition is a fixed time interval
	 * between transmissions. */
	struct k_timer trigger_timer;
};

/* GATT read ESS characteristic's value (sint16). */
static ssize_t chrc_gatt_read_sint16(struct ess_characteristic const *chrc, struct bt_conn *conn,
				     struct bt_gatt_attr const *attr, void *buf, uint16_t len,
				     uint16_t offset)
{
	atomic_val_t val = atomic_get(&chrc->value);
	int16_t val_le = sys_cpu_to_le16((int16_t)val);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val_le, sizeof(val_le));
}

/* GATT read ESS characteristic's value (uint16). */
static ssize_t chrc_gatt_read_uint16(struct ess_characteristic const *chrc, struct bt_conn *conn,
				     struct bt_gatt_attr const *attr, void *buf, uint16_t len,
				     uint16_t offset)
{
	atomic_val_t val = atomic_get(&chrc->value);
	uint16_t val_le = sys_cpu_to_le16((uint16_t)val);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val_le, sizeof(val_le));
}

/* GATT read ESS characteristic's value (uint32). */
static ssize_t chrc_gatt_read_uint32(struct ess_characteristic const *chrc, struct bt_conn *conn,
				     struct bt_gatt_attr const *attr, void *buf, uint16_t len,
				     uint16_t offset)
{
	atomic_val_t val = atomic_get(&chrc->value);
	uint32_t val_le = sys_cpu_to_le32((uint32_t)val);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val_le, sizeof(val_le));
}

/* GATT read ESS characteristic's trigger setting (no operand). */
static ssize_t gatt_read_trigger_setting_operand_na(struct es_trigger_setting const *setting,
						    struct bt_conn *conn,
						    struct bt_gatt_attr const *attr, void *buf,
						    uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1] = {setting->condition};
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (time-based). */
static ssize_t gatt_read_trigger_setting_seconds(struct es_trigger_setting const *setting,
						 struct bt_conn *conn,
						 struct bt_gatt_attr const *attr, void *buf,
						 uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + 3] = {setting->condition};
	sys_put_le24(setting->operand.seconds, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, sint16). */
static ssize_t gatt_read_trigger_setting_sint16(struct es_trigger_setting const *setting,
						struct bt_conn *conn,
						struct bt_gatt_attr const *attr, void *buf,
						uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + sizeof(setting->operand.val_sint16)] = {setting->condition};
	sys_put_le16(setting->operand.val_sint16, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, uint32_t). */
static ssize_t gatt_read_trigger_setting_uint32(struct es_trigger_setting const *setting,
						struct bt_conn *conn,
						struct bt_gatt_attr const *attr, void *buf,
						uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + sizeof(setting->operand.val_uint32)] = {setting->condition};
	sys_put_le32(setting->operand.val_uint32, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* GATT read ESS characteristic's trigger setting (value-based, uint16_t). */
static ssize_t gatt_read_trigger_setting_uint16(struct es_trigger_setting const *setting,
						struct bt_conn *conn,
						struct bt_gatt_attr const *attr, void *buf,
						uint16_t len, uint16_t offset)
{
	uint8_t attr_value[1 + sizeof(setting->operand.val_uint16)] = {setting->condition};
	sys_put_le16(setting->operand.val_uint16, &attr_value[1]);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr_value, sizeof(attr_value));
}

/* Check ESS characteristic's trigger setting condition (time-based, not less than seconds). */
static bool chrc_trigger_setting_check_time(struct ess_characteristic *chrc)
{
	if (chrc->trigger_setting.condition == ES_TRIGGER_GTE_TIME) {
		uint32_t ts = k_uptime_seconds();
		LOG_DBG("%u - %u >= %u ?", ts, chrc->value_ts,
			chrc->trigger_setting.operand.seconds);

		if (ts - chrc->value_ts >= chrc->trigger_setting.operand.seconds) {
			chrc->value_ts = ts;
			return true;
		}
	}
	return false;
}

/* Check ESS characteristic's trigger setting condition (value-based, sint16). */
static bool chrc_trigger_setting_check_sint16(struct ess_characteristic const *chrc, int16_t value,
					      int16_t old_value)
{
	switch (chrc->trigger_setting.condition) {
	/* Value-based conditions. */
	case ES_TRIGGER_VALUE_CHANGED:
		return value != old_value;
	case ES_TRIGGER_LT_VALUE:
		return value < chrc->trigger_setting.operand.val_sint16;
	case ES_TRIGGER_LTE_VALUE:
		return value <= chrc->trigger_setting.operand.val_sint16;
	case ES_TRIGGER_GT_VALUE:
		return value > chrc->trigger_setting.operand.val_sint16;
	case ES_TRIGGER_GTE_VALUE:
		return value >= chrc->trigger_setting.operand.val_sint16;
	case ES_TRIGGER_SPECIFIED_VALUE:
		return value == chrc->trigger_setting.operand.val_sint16;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return value != chrc->trigger_setting.operand.val_sint16;
	/* Unrelated conditions. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		__fallthrough;
	default:
		return false;
	}
}

/* Check ESS characteristic's trigger setting condition (value-based, uint16). */
static bool chrc_trigger_setting_check_uint16(struct ess_characteristic const *chrc, uint16_t value,
					      uint16_t old_value)
{
	switch (chrc->trigger_setting.condition) {
	/* Value-based conditions. */
	case ES_TRIGGER_VALUE_CHANGED:
		return value != old_value;
	case ES_TRIGGER_LT_VALUE:
		return value < chrc->trigger_setting.operand.val_uint16;
	case ES_TRIGGER_LTE_VALUE:
		return value <= chrc->trigger_setting.operand.val_uint16;
	case ES_TRIGGER_GT_VALUE:
		return value > chrc->trigger_setting.operand.val_uint16;
	case ES_TRIGGER_GTE_VALUE:
		return value >= chrc->trigger_setting.operand.val_uint16;
	case ES_TRIGGER_SPECIFIED_VALUE:
		return value == chrc->trigger_setting.operand.val_uint16;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return value != chrc->trigger_setting.operand.val_uint16;
	/* Unrelated conditions. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		__fallthrough;
	default:
		return false;
	}
}

/* Check ESS characteristic's trigger setting condition (value-based, uint32). */
static bool chrc_trigger_setting_check_uint32(struct ess_characteristic const *chrc, uint32_t value,
					      uint32_t old_value)
{
	switch (chrc->trigger_setting.condition) {
	/* Value-based conditions. */
	case ES_TRIGGER_VALUE_CHANGED:
		return value != old_value;
	case ES_TRIGGER_LT_VALUE:
		return value < chrc->trigger_setting.operand.val_uint32;
	case ES_TRIGGER_LTE_VALUE:
		return value <= chrc->trigger_setting.operand.val_uint32;
	case ES_TRIGGER_GT_VALUE:
		return value > chrc->trigger_setting.operand.val_uint32;
	case ES_TRIGGER_GTE_VALUE:
		return value >= chrc->trigger_setting.operand.val_uint32;
	case ES_TRIGGER_SPECIFIED_VALUE:
		return value == chrc->trigger_setting.operand.val_uint32;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return value != chrc->trigger_setting.operand.val_uint32;
	/* Unrelated conditions. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		__fallthrough;
	default:
		return false;
	}
}

/* Wrapper logging notification errors. */
static void gatt_notify_peers(struct bt_gatt_attr const *attr, void const *data, uint16_t len)
{
	int err = bt_gatt_notify(NULL, attr, data, len);

	char uuid_str[BT_UUID_STR_LEN];
	bt_uuid_to_str(attr->uuid, uuid_str, sizeof(uuid_str));

	/*
	 * NOTE: We should not get -ENOTCONN as long as we're called
	 * only for characteristics with ess_characteristic.ccc_notify set.
	 */
	if (err) {
		LOG_ERR("%s: notification error %d", uuid_str, err);
	} else {
		LOG_DBG("%s", uuid_str);
	}
}

/* GATT notify ESS characteristic with known value. */
static void chrc_gatt_notify(struct ess_characteristic const *chrc, void const *data, uint16_t len)
{
	struct bt_gatt_attr const *attr = bt_gatt_find_by_uuid(NULL, 0, chrc->uuid);
	if (attr) {
		gatt_notify_peers(attr, data, len);
	} else {
		LOG_ERR("notification: unregistered characteristic %p", (void *)chrc);
	}
}

/* GATT notify ESS characteristic's value (sint16). */
static void chrc_gatt_notify_sint16(struct ess_characteristic const *chrc)
{
	atomic_val_t val = atomic_get(&chrc->value);
	int16_t val_le = sys_cpu_to_le16((int16_t)val);
	chrc_gatt_notify(chrc, &val_le, sizeof(val_le));
}

/* GATT notify ESS characteristic's value (uint16). */
static void chrc_gatt_notify_uint16(struct ess_characteristic const *chrc)
{
	atomic_val_t val = atomic_get(&chrc->value);
	uint16_t val_le = sys_cpu_to_le16((uint16_t)val);
	chrc_gatt_notify(chrc, &val_le, sizeof(val_le));
}

/* GATT notify ESS characteristic's value (uint32). */
static void chrc_gatt_notify_uint32(struct ess_characteristic const *chrc)
{
	atomic_val_t val = atomic_get(&chrc->value);
	uint32_t val_le = sys_cpu_to_le32((uint32_t)val);
	chrc_gatt_notify(chrc, &val_le, sizeof(val_le));
}

/* Update ESS characteristic's value (sint16), notify peers when appropriate. */
static void chrc_gatt_write_value_sint16(struct ess_characteristic *chrc, int16_t value)
{
	int16_t old_value = atomic_set(&chrc->value, value);

	if (atomic_get(&chrc->ccc_notify) == 0) {
		return;
	}

	bool chk_notify = false;
	if (chrc->trigger_setting.condition == ES_TRIGGER_INACTIVE) {
		chk_notify = true;
	} else if (chrc->trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		chk_notify = chrc_trigger_setting_check_time(chrc);
	} else {
		chk_notify = chrc_trigger_setting_check_sint16(chrc, value, old_value);
	}

	if (chk_notify) {
		chrc_gatt_notify_sint16(chrc);
	}
}

/* Update ESS characteristic's value (uint16), notify peers when appropriate. */
static void chrc_gatt_write_value_uint16(struct ess_characteristic *chrc, uint16_t value)
{
	uint16_t old_value = atomic_set(&chrc->value, value);

	if (atomic_get(&chrc->ccc_notify) == 0) {
		return;
	}

	bool chk_notify = false;
	if (chrc->trigger_setting.condition == ES_TRIGGER_INACTIVE) {
		chk_notify = true;
	} else if (chrc->trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		chk_notify = chrc_trigger_setting_check_time(chrc);
	} else {
		chk_notify = chrc_trigger_setting_check_uint16(chrc, value, old_value);
	}

	if (chk_notify) {
		chrc_gatt_notify_uint16(chrc);
	}
}

/* Update ESS characteristic's value (uint32), notify peers when appropriate. */
static void chrc_gatt_write_value_uint32(struct ess_characteristic *chrc, uint32_t value)
{
	uint32_t old_value = atomic_set(&chrc->value, value);

	if (atomic_get(&chrc->ccc_notify) == 0) {
		return;
	}

	bool chk_notify = false;
	if (chrc->trigger_setting.condition == ES_TRIGGER_INACTIVE) {
		chk_notify = true;
	} else if (chrc->trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		chk_notify = chrc_trigger_setting_check_time(chrc);
	} else {
		chk_notify = chrc_trigger_setting_check_uint32(chrc, value, old_value);
	}

	if (chk_notify) {
		chrc_gatt_notify_uint32(chrc);
	}
}

/* CCC changed event, update whether at least connected peer
 * has enabled notifications for a given characteristic. */
static bool chrc_ccc_changed(struct ess_characteristic *chrc, uint16_t value)
{
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set(&chrc->ccc_notify, 1);
		return true;
	}

	atomic_clear(&chrc->ccc_notify);
	return false;
}

/* CCC changed event, start/stop periodic notification's timer
 * depending on new CCC value. */
static void chrc_trigger_timer_start_stop(struct ess_characteristic *chrc, bool ccc_notify)
{
	if (ccc_notify) {
		/* Start periodic notifications. */
		k_timeout_t timeout = K_SECONDS(chrc->trigger_setting.operand.seconds);
		k_timer_start(&chrc->trigger_timer, timeout, timeout);

		LOG_DBG("CCC timer start (%u secs)", chrc->trigger_setting.operand.seconds);

	} else {
		/* Stop periodic notifications. */
		k_timer_stop(&chrc->trigger_timer);

		LOG_DBG("CCC timer stop");
	}
}

static void chrc_temperature_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value);
static ssize_t chrc_temperature_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					   void *buf, uint16_t len, uint16_t offset);
static ssize_t chrc_temperature_read_trigger_setting(struct bt_conn *conn,
						     struct bt_gatt_attr const *attr, void *buf,
						     uint16_t len, uint16_t offset);

static void chrc_pressure_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value);
static ssize_t chrc_pressure_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					void *buf, uint16_t len, uint16_t offset);
static ssize_t chrc_pressure_read_trigger_setting(struct bt_conn *conn,
						  struct bt_gatt_attr const *attr, void *buf,
						  uint16_t len, uint16_t offset);

static void chrc_humidity_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value);
static ssize_t chrc_humidity_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					void *buf, uint16_t len, uint16_t offset);
static ssize_t chrc_humidity_read_trigger_setting(struct bt_conn *conn,
						  struct bt_gatt_attr const *attr, void *buf,
						  uint16_t len, uint16_t offset);

/* Client Presentation Format - Temperature (GATT_SS§3.218). */
static struct bt_gatt_cpf const temperature_cpf = {
	/* AN§2.4.1 GATT Format Types: sint16. */
	.format = 0x0E,
	/* Represented values: M = 1, d = -2, b = 0 */
	.exponent = -2,
	/* AN§3.5 Units: Celsius temperature (degree Celsius). */
	.unit = 0x272F,
	/* Bluetooth SIG */
	.name_space = 0x01,
	/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main". */
	.description = 0x0106,
};

/* Client Presentation Format - Pressure (GATT_SS§3.181). */
static struct bt_gatt_cpf const pressure_cpf = {
	/* AN§2.4.1 GATT Format Types: uint32. */
	.format = 0x08,
	/* Represented values: M = 1, d = -1, b = 0 */
	.exponent = -1,
	/* AN§3.5 Units: Pressure (Pascal). */
	.unit = 0x2724,
	/* Bluetooth SIG. */
	.name_space = 0x01,
	/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main". */
	.description = 0x0106,
};

/* Client Presentation Format - Humidity (GATT_SS§3.124). */
static struct bt_gatt_cpf const humidity_cpf = {
	/* AN§2.4.1 GATT Format Types: uint16. */
	.format = 0x06,
	/* Represented values: M = 1, d = -2, b = 0 */
	.exponent = -2,
	/* AN§3.5 Units: Percentage. */
	.unit = 0x27AD,
	/* Bluetooth SIG. */
	.name_space = 0x01,
	/* AN§2.4.2.1 GATT Characteristic Presentation Format Description: "main". */
	.description = 0x0106,
};

static struct ess_characteristic ess_chrc_temperature = {
	.uuid = BT_UUID_TEMPERATURE,
	.value = ATOMIC_INIT(ESS_GATT_TEMPERATURE_UNKNOWN),
	.trigger_setting = {.condition = ES_TRIGGER_INACTIVE},
};

static struct ess_characteristic ess_chrc_pressure = {
	.uuid = BT_UUID_PRESSURE,
	.trigger_setting = {.condition = ES_TRIGGER_INACTIVE},
};

static struct ess_characteristic ess_chrc_humidity = {
	.uuid = BT_UUID_HUMIDITY,
	.value = ATOMIC_INIT(ESS_GATT_HUMIDITY_UNKNOWN),
	.trigger_setting = {.condition = ES_TRIGGER_INACTIVE},
};

BT_GATT_SERVICE_DEFINE(
	ess_srv, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
	/* Temperature.*/
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, chrc_temperature_read_value, NULL, NULL),
	BT_GATT_CCC(chrc_temperature_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CPF(&temperature_cpf),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			   chrc_temperature_read_trigger_setting, NULL, NULL),
	/* Pressure.*/
	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, chrc_pressure_read_value, NULL, NULL),
	BT_GATT_CCC(chrc_pressure_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CPF(&pressure_cpf),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING, BT_GATT_PERM_READ,
			   chrc_pressure_read_trigger_setting, NULL, NULL),
	/* Humidity.*/
	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, chrc_humidity_read_value, NULL, NULL),
	BT_GATT_CCC(chrc_humidity_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CPF(&humidity_cpf),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_TRIGGER_SETTING, BT_GATT_PERM_READ,
			   chrc_humidity_read_trigger_setting, NULL, NULL));

ssize_t chrc_temperature_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr,
				    void *buf, uint16_t len, uint16_t offset)
{
	return chrc_gatt_read_sint16(&ess_chrc_temperature, conn, attr, buf, len, offset);
}

ssize_t chrc_temperature_read_trigger_setting(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					      void *buf, uint16_t len, uint16_t offset)
{
	switch (ess_chrc_temperature.trigger_setting.condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		return gatt_read_trigger_setting_operand_na(&ess_chrc_temperature.trigger_setting,
							    conn, attr, buf, len, offset);

	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		return gatt_read_trigger_setting_seconds(&ess_chrc_temperature.trigger_setting,
							 conn, attr, buf, len, offset);

	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return gatt_read_trigger_setting_sint16(&ess_chrc_temperature.trigger_setting, conn,
							attr, buf, len, offset);

	default:
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
}

void chrc_temperature_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value)
{
	bool ccc_notify = chrc_ccc_changed(&ess_chrc_temperature, value);
	LOG_DBG("notify:%u", ccc_notify);

	if (ess_chrc_temperature.trigger_setting.condition == ES_TRIGGER_FIXED_TIME) {
		chrc_trigger_timer_start_stop(&ess_chrc_temperature, ccc_notify);
	}
}

static void chrc_temperature_trigger_timeout(struct k_timer *timer)
{
	/* Periodic notification, no configuration nor state to check. */
	chrc_gatt_notify_sint16(&ess_chrc_temperature);
}

static void chrc_temperature_trigger_setting_init(void)
{
	LOG_INF("Temperature trigger: 0x%02x (%d)", TEMPERATURE_TRIGGER_CONDITION,
		TEMPERATURE_TRIGGER_OPERAND_VALUE);

	k_timer_init(&ess_chrc_temperature.trigger_timer, chrc_temperature_trigger_timeout, NULL);
	ess_chrc_temperature.trigger_setting.condition = TEMPERATURE_TRIGGER_CONDITION;

	if ((ess_chrc_temperature.trigger_setting.condition == ES_TRIGGER_INACTIVE)
	    || (ess_chrc_temperature.trigger_setting.condition == ES_TRIGGER_VALUE_CHANGED)) {
		/* No operand to initialize. */
		return;
	}

	if (ess_chrc_temperature.trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		/* Time-based conditions. */
		ess_chrc_temperature.trigger_setting.operand.seconds =
			TEMPERATURE_TRIGGER_OPERAND_VALUE;
	} else {
		/* Value-based conditions (sint16). */
		ess_chrc_temperature.trigger_setting.operand.val_sint16 =
			TEMPERATURE_TRIGGER_OPERAND_VALUE;
	}
}

ssize_t chrc_pressure_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return chrc_gatt_read_uint32(&ess_chrc_pressure, conn, attr, buf, len, offset);
}

ssize_t chrc_pressure_read_trigger_setting(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					   void *buf, uint16_t len, uint16_t offset)
{
	switch (ess_chrc_pressure.trigger_setting.condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		return gatt_read_trigger_setting_operand_na(&ess_chrc_pressure.trigger_setting,
							    conn, attr, buf, len, offset);

	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		return gatt_read_trigger_setting_seconds(&ess_chrc_pressure.trigger_setting, conn,
							 attr, buf, len, offset);

	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return gatt_read_trigger_setting_uint32(&ess_chrc_pressure.trigger_setting, conn,
							attr, buf, len, offset);

	default:
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
}

void chrc_pressure_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value)
{
	bool ccc_notify = chrc_ccc_changed(&ess_chrc_pressure, value);
	LOG_DBG("notify:%u", ccc_notify);

	if (ess_chrc_pressure.trigger_setting.condition == ES_TRIGGER_FIXED_TIME) {
		chrc_trigger_timer_start_stop(&ess_chrc_pressure, ccc_notify);
	}
}

static void chrc_pressure_trigger_timeout(struct k_timer *timer)
{
	/* Periodic notification, no configuration nor state to check. */
	chrc_gatt_notify_uint32(&ess_chrc_pressure);
}

static void chrc_pressure_trigger_setting_init(void)
{
	LOG_INF("Presure trigger: 0x%02x (%d)", PRESSURE_TRIGGER_CONDITION,
		PRESSURE_TRIGGER_OPERAND_VALUE);

	k_timer_init(&ess_chrc_pressure.trigger_timer, chrc_pressure_trigger_timeout, NULL);
	ess_chrc_pressure.trigger_setting.condition = PRESSURE_TRIGGER_CONDITION;

	if ((ess_chrc_pressure.trigger_setting.condition == ES_TRIGGER_INACTIVE)
	    || (ess_chrc_pressure.trigger_setting.condition == ES_TRIGGER_VALUE_CHANGED)) {
		/* No operand to initialize. */
		return;
	}

	if (ess_chrc_pressure.trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		/* Time-based conditions. */
		ess_chrc_pressure.trigger_setting.operand.seconds = PRESSURE_TRIGGER_OPERAND_VALUE;
	} else {
		/* Value-based conditions (uint32). */
		ess_chrc_pressure.trigger_setting.operand.val_uint32 =
			PRESSURE_TRIGGER_OPERAND_VALUE;
	}
}

ssize_t chrc_humidity_read_value(struct bt_conn *conn, struct bt_gatt_attr const *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return chrc_gatt_read_uint16(&ess_chrc_humidity, conn, attr, buf, len, offset);
}

ssize_t chrc_humidity_read_trigger_setting(struct bt_conn *conn, struct bt_gatt_attr const *attr,
					   void *buf, uint16_t len, uint16_t offset)
{
	switch (ess_chrc_humidity.trigger_setting.condition) {
	/* Conditions without operand. */
	case ES_TRIGGER_INACTIVE:
		__fallthrough;
	case ES_TRIGGER_VALUE_CHANGED:
		return gatt_read_trigger_setting_operand_na(&ess_chrc_humidity.trigger_setting,
							    conn, attr, buf, len, offset);

	/* Time-based conditions. */
	case ES_TRIGGER_FIXED_TIME:
		__fallthrough;
	case ES_TRIGGER_GTE_TIME:
		return gatt_read_trigger_setting_seconds(&ess_chrc_humidity.trigger_setting, conn,
							 attr, buf, len, offset);

	/* Value-based conditions. */
	case ES_TRIGGER_LT_VALUE:
		__fallthrough;
	case ES_TRIGGER_LTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_GT_VALUE:
		__fallthrough;
	case ES_TRIGGER_GTE_VALUE:
		__fallthrough;
	case ES_TRIGGER_SPECIFIED_VALUE:
		__fallthrough;
	case ES_TRIGGER_NOT_SPECIFIED_VALUE:
		return gatt_read_trigger_setting_uint16(&ess_chrc_humidity.trigger_setting, conn,
							attr, buf, len, offset);

	default:
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
}

void chrc_humidity_ccc_changed(struct bt_gatt_attr const *attr, uint16_t value)
{
	bool ccc_notify = chrc_ccc_changed(&ess_chrc_humidity, value);
	LOG_DBG("notify:%u", ccc_notify);

	if (ess_chrc_humidity.trigger_setting.condition == ES_TRIGGER_FIXED_TIME) {
		chrc_trigger_timer_start_stop(&ess_chrc_humidity, ccc_notify);
	}
}

static void chrc_humidity_trigger_timeout(struct k_timer *timer)
{
	/* Periodic notification, no configuration nor state to check. */
	chrc_gatt_notify_uint16(&ess_chrc_humidity);
}

static void chrc_humidity_trigger_setting_init(void)
{
	LOG_INF("Humidity trigger: 0x%02x (%d)", HUMIDITY_TRIGGER_CONDITION,
		HUMIDITY_TRIGGER_OPERAND_VALUE);

	k_timer_init(&ess_chrc_humidity.trigger_timer, chrc_humidity_trigger_timeout, NULL);
	ess_chrc_humidity.trigger_setting.condition = HUMIDITY_TRIGGER_CONDITION;

	if ((ess_chrc_humidity.trigger_setting.condition == ES_TRIGGER_INACTIVE)
	    || (ess_chrc_humidity.trigger_setting.condition == ES_TRIGGER_VALUE_CHANGED)) {
		/* No operand to initialize. */
		return;
	}

	if (ess_chrc_humidity.trigger_setting.condition < ES_TRIGGER_VALUE_CHANGED) {
		/* Time-based conditions. */
		ess_chrc_humidity.trigger_setting.operand.seconds = HUMIDITY_TRIGGER_OPERAND_VALUE;
	} else {
		/* Value-based conditions (uint16). */
		ess_chrc_humidity.trigger_setting.operand.val_uint16 =
			HUMIDITY_TRIGGER_OPERAND_VALUE;
	}
}

int bme68x_ess_init(void)
{
	chrc_temperature_trigger_setting_init();
	chrc_pressure_trigger_setting_init();
	chrc_humidity_trigger_setting_init();
	return 0;
}

int bme68x_ess_update_temperature(int16_t temperature)
{
	if ((temperature < -27315) && (temperature != ESS_GATT_TEMPERATURE_UNKNOWN)) {
		LOG_WRN("invalid temperature: %d", temperature);
		return -EINVAL;
	}

	chrc_gatt_write_value_sint16(&ess_chrc_temperature, temperature);
	return 0;
}

int bme68x_ess_update_pressure(uint32_t pressure)
{
	chrc_gatt_write_value_uint32(&ess_chrc_pressure, pressure);
	return 0;
}

int bme68x_ess_update_humidity(uint16_t humidity)
{
	if ((humidity > 10000) && (humidity != ESS_GATT_HUMIDITY_UNKNOWN)) {
		LOG_WRN("invalid humidity: %d", humidity);
		return -EINVAL;
	}

	chrc_gatt_write_value_uint16(&ess_chrc_humidity, humidity);
	return 0;
}
