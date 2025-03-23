# `lib/bme68x-esp` ─ Support library for the Bluetooth Environmental Sensing Profile (ESP)

Support library for the Bluetooth Environmental Sensing Profile (ESP).

Primary focus is the Environmental Sensing Service (ESS) of the Environmental Sensor role:

- Supports the Temperature, Pressure and Humidity ESS characteristics
- For each characteristic, an ES Trigger Setting can be configured (all conditions are supported)

> [!NOTE]
>
> This library does not depend on the [BME68X Sensor API] provided by [lib/bme68x-sensor-api] nor assume the use of BME68X devices.

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[lib/bme68x-sensor-api]: lib/bme68x-sensor-api

References:
- [Environmental Sensing Profile 1.0]
- [Environmental Sensing Service 1.0]

[Environmental Sensing Profile 1.0]: https://www.bluetooth.com/specifications/specs/environmental-sensing-profile-1-0/
[Environmental Sensing Service 1.0]: https://www.bluetooth.com/specifications/specs/environmental-sensing-service-1-0/

## Configuration

This library depends on the Bluetooth LE Peripheral role: see [`conf/ble_peripheral.conf`] for an example configuration file.

To enable this library:

| [`Kconfig`]               | Option                                     |
|---------------------------|--------------------------------------------|
| `BME68X_ESP`              | Enable Bluetooth Environmental Sensor role |
| `BME68X_ESP_GAP_ADV_AUTO` | Automatically resume advertising           |

Enabling this library will automatically expose the Environmental Sensing Service (ESS).

Notifications for each supported ESS characteristic can be configured with an ES Trigger Settings, as described in Bluetooth Environmental Sensing Service §3.1.2.2: see [`conf/es_notifications.conf`] for examples.

All configuration options are accessible via the Kconfig menu: `Modules → bme68x → [*] Environmental Sensing Profile`.

[`Kconfig`]: Kconfig
[`conf/es_peripheral.conf`]: conf/es_peripheral.conf
[`conf/es_notifications.conf`]: conf/es_notifications.conf

## API

| API                     | Description                       |
|-------------------------|-----------------------------------|
| [`bme68x_ess.h`]        | Environmental Sensing Service     |
| [`bme68x_esp_gap.h`]    | Connections management            |
| [`bme68x_esp_sensor.h`] |  |

[`bme68x_ess.h`]: include/bme68x_ess.h
[`bme68x_esp_gap.h`]: include/bme68x_esp_gap.h
[`bme68x_esp_sensor.h`]: include/bme68x_esp_sensor.h
