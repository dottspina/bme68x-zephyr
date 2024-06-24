# `lib/bme68x-sensor-api` ─ BME68X Sensor API

The [BME68X Sensor API] is an open source library provided by Bosch Sensortec, which covers configuration and control of BME680/688 sensors, as well as compensation of temperature, pressure, humidity and gas resistance values.

See also:

- [drivers/bme68x-sensor-API]: integration with Zephyr Devicetree and I2C/SPI drivers
- [samples/bme68x-tphg]: example application, forced temperature, pressure, humidity and gas resistance measurements with the BME68X Sensor API
- [samples/bme68x-iaq]: example application, Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API


> [!NOTE]
>
> The BME688 sensor hardware is the same as BME680 except that:
>
> - it can measure higher gas resistance
> - it adds a gas scanner function
>
> See also [How to distinguish BME680 from BME688 in firmware].

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[drivers/bme68x-sensor-api]: /drivers/bme68x-sensor-api
[samples/bme68x-tphg]: /samples/bme68x-tphg
[samples/bme68x-iaq]: /samples/bme68x-iaq
[How to distinguish BME680 from BME688 in firmware]: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/How-to-distinguish-BME680-from-BME688-in-firmware/td-p/73929

## BME68X Sensor API

This Zephyr library provides [BME68X Sensor API v4.4.8].

When enabled (`BME68X_SENSOR_API=y`), the usual BME68X Sensor API header files are directly accessible by application code.

| Header                                  | API                                          |
|-----------------------------------------|----------------------------------------------|
| [`bme68x_def.h`](include/bme68x_defs.h) | BME68X Sensor API definitions and data types |
| [`bme68x.h`](include/bme68x.h)          | BME68X Sensor API interface                  |

The BME68X Sensor API comes in two variants: the *default* one, with floating-point data output, and another using only integer data types, selected when `BME68X_DO_NOT_USE_FPU` is defined *somewhere*.

This library overrides the default behavior to:

- consistently select the API variant *once and for all* with Kconfig
- prefer the fixed-point API unless explicitly asked to with the option `BME68X_SENSOR_API_FLOAT`

> [!NOTE]
>
> Preference for the floating point API does not imply or depend on [`FPU`] or hardware floating-point ABI.
>
> Applications should rely on `BME68X_SENSOR_API_FLOAT` rather than `BME68X_USE_FPU` to avoid this confusion, e.g.:
>
> ``` C
> #if BME68X_SENSOR_API_FLOAT
>     float temp_degC = bme68x_data.temperature;
> #else
>     int16_t temp_degC_x100 = bme68x_data.temperature;
> #endif
> ```

[BME68X Sensor API v4.4.8]: https://github.com/boschsensortec/BME68x_SensorAPI/releases/tag/v4.4.8
[`FPU`]: https://docs.zephyrproject.org/latest/kconfig.html#CONFIG_FPU

## Kconfig

Software configuration with [Kconfig].

| [`Kconfig`](Kconfig)      | Configuration             |
|---------------------------|---------------------------|
| `BME68X_SENSOR_API`       | Enable BME68X Sensor API  |
| `BME68X_SENSOR_API_FLOAT` | Prefer floating-point API |

[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html

> [!TIP]
>
> This library is automatically enabled by [drivers/bme68x-sensor-API].
