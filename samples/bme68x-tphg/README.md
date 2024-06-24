# BME68X Sample ─ TPHG measurements with the BME68X Sensor API

Sample application for periodic temperature, pressure, humidity and gas resistance measurements, in forced mode with a single heater set-point.

This is basically a port of the [forced mode example] program that comes with the [BME68X Sensor API].

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[forced mode example]: https://github.com/boschsensortec/BME68x_SensorAPI/blob/master/examples/forced_mode/forced_mode.c

> [!TIP]
>
> Use this application to test [communication interface] configurations.

[communication interface]: /drivers/bme68x-sensor-api/README.md#compatible-devices

## Configuration

### Communication interface

This sample application relies on [drivers/bme68x-sensor-api] to communicate with the BME680/688 device.

The communication interface must be described in the devicetree, typically with an overlay file, e.g.:

``` dts
/* I2C bus the BME680/688 device is connected to. */
&i2c0 {
    bme680@76 {
        compatible = "bosch,bme68x-sensor-api";

        /* BME68X I2C slave address (0x76 or 0x77) */
        reg = < 0x76 >;
    };
};
```

See also [Use devicetree overlays] if this does not sound familiar.

[drivers/bme68x-sensor-api]: /drivers/bme68x-sensor-api
[Use devicetree overlays]: https://docs.zephyrproject.org/latest/build/dts/howtos.html#use-devicetree-overlays

### Sensor

The sensor is configured with [Kconfig] options.

| [`Kconfig`](Kconfig)                            | Configuration                        |
|-------------------------------------------------|--------------------------------------|
| `BME68X_TPHG_SAMPLE_RATE`                       | Measurements period (seconds)        |
| `BME68X_TPHG_AMBIENT_TEMP (=25)`                | Initial ambient temperature estimate |
| `BME68X_TPHG_TEMP_OS_{NONE,1X,...,16X} (=2X)`   | Temperature oversampling             |
| `BME68X_TPHG_PRESS_OS_{NONE,1X,...,16X} (=16X)` | Pressure oversampling                |
| `BME68X_TPHG_HUM_OS_{NONE,1X,...,16X} (=1X)`    | Humidity oversampling                |
| `BME68X_TPHG_FILTER_{OFF,...,128} (=OFF)`       | IIR filter                           |
| `BME68X_TPHG_HEATR_TEMP (=320)`                 | Heater set-point in degree Celsius   |
| `BME68X_TPHG_HEATR_DUR (=197)`                  | Heating duration in millisecond      |
| `BME68X_TPHG_LOG_LEVEL`                         | Application log level                |

For example, in `prj.conf`:

```
# Enable IIR filter
CONFIG_BME68X_TPHG_FILTER_2=y

# ULP mode.
CONFIG_BME68X_TPHG_ULP=y
```

Configuration options are also accessible via the Kconfig menu: `BME68X Sample - TPHG`

> [!TIP]
>
> The default configuration should be just fine for testing purpose.

[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html


## Building and running

Nothing unusual, e:g:

```
$ cd bme68x-zephyr
$ west build samples/bme68x-tphg -t menuconfig
$ west flash
```

Console output:

```
*** Booting Zephyr OS build v3.6.0 ***
[00:00:00.257,263] <inf> bme68x_sensor_api: bme680@77 (Fixed-point API)
[00:00:00.294,769] <inf> bme68x_tphg: os_t:x2 os_p:x16 os_h:x1 iir:off
[00:00:00.298,034] <inf> bme68x_tphg: heatr_temp:320 degC  heatr_dur:197 ms
[00:00:00.298,065] <inf> bme68x_tphg: TPHG cycle: 239590 us
[00:00:00.542,602] <inf> bme68x_tphg: T:25.86 deg C, P:101.011 kPa, H:51.381 %, G:38.524 kOhm
[00:00:03.787,231] <inf> bme68x_tphg: T:25.90 deg C, P:101.011 kPa, H:51.481 %, G:35.511 kOhm
[00:00:07.031,890] <inf> bme68x_tphg: T:25.94 deg C, P:101.013 kPa, H:51.468 %, G:35.271 kOhm
[00:00:10.276,550] <inf> bme68x_tphg: T:25.95 deg C, P:101.013 kPa, H:51.394 %, G:35.360 kOhm
[00:00:13.521,209] <inf> bme68x_tphg: T:25.96 deg C, P:101.009 kPa, H:51.328 %, G:35.271 kOhm
```

> [!TIP]
>
> If the `bme68x` module is not installed as an external project managed by West, its path must be appended to `ZEPHYR_EXTRA_MODULES`:
>
> - in [`CMakeLists.txt`]
> - or as CMake variable, e.g. `west build -- -DZEPHYR_EXTRA_MODULES=/path/to/bme68x-zephyr`

[`CMakeLists.txt`]: CMakeLists.txt
