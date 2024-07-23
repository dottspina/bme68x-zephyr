# `drivers/bme68x-sensor-api` ─ BME68X Sensor API (Driver)

The [BME68X Sensor API] is an open source library provided by Bosch Sensortec, which covers configuration and control of BME680/688 sensors, as well as compensation of temperature, pressure, humidity and gas resistance values.

Most documented uses of BME680/688 sensors, including BSEC integration for Index for Air Quality (IAQ) and gas detection, assume that this library is available in some form, e.g. the [Bosch-BME68x-Library] for Arduino.

Porting this library essentially means providing the platform-specific communication interfaces (I2C/SPI) that the BME68X Sensor API implementation eventually relies on for accessing sensor registers.

In Zephyr context, this also requires integration with Devicetree, that is best addressed at the driver layer.

See also:

- [samples/bme68x-tphg]: PoC application, forced temperature, pressure, humidity and gas resistance measurements with the BME68X Sensor API
- [samples/bme68x-iaq]: example application, Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[Bosch-BME68x-Library]: https://github.com/boschsensortec/Bosch-BME68x-Library
[lib/bme68x-sensor-api]: /lib/bme68x-sensor-api
[samples/bme68x-tphg]: /samples/bme68x-tphg
[samples/bme68x-iaq]: /samples/bme68x-iaq

## API

Compatible devices (see [Compatible Devices](#compatible-devices)) are intended for use with the BME68X Sensor API, in particular:

- they don't provide the Zephyr [Sensors API]: this approach is covered by the upstream [BME680 driver], and the use cases considered here are those that we found impractical, if not impossible, to implement on top of the Sensors API
- the driver defines a single API entry point, `bme68x_sensor_api_init()`, which permits applications to *bind* BME68X Sensor API sensor instances to Zephyr device driver instances

| Header                          | API                                                  |
|---------------------------------|------------------------------------------------------|
| [`drivers/bme68x_sensor_api.h`] | Bind BME68X Sensor API sensors to compatible devices |

[Sensors API]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor.html#sensors
[BME680 driver]: https://docs.zephyrproject.org/latest/samples/sensor/bme680/README.html
[`drivers/bme68x_sensor_api.h`]: include/drivers/bme68x_sensor_api.h
[`bme68x_def.h`]: /lib/bme68x-sensor-api/include/bme68x_def.h
[`bme68x.h`]: /lib/bme68x-sensor-api/include/bme68x.h

``` C
/* Zephyr Device Driver Model */
#include <zephyr/device.h>

/* BME68X Sensor API (Driver). */
#include <drivers/bme68x_sensor_api.h>

/* BME68X Sensor API. */
#include "bme68x.h"

/* Compatible device driver instance (see Devicetree HOWTOs). */
static struct device const *const dev = DEVICE_DT_GET_ONE(bosch_bme68x_sensor_api);

/* BME68X Sensor API sensor. */
static struct bme68x_dev bme68x_dev;

int main()
{
    /*
     * Zephyr integration: bind device to sensor.
     */
    int ret = bme68x_sensor_api_init(dev, &bme68x_dev);
    if (ret == -ENODEV) {
        return 0;
    }

    /*
     * Usual BME68X Sensor API sensor initialization.
     */
    ret = bme68x_init(&bme68x_dev);

    if (ret == BME68X_OK) {
        /*
         * Application based on BME68X Sensor API:
         * - TPHG measurements
         * - BSEC
         */
    }

    return 0;
}
```

Beside that, this driver follows Zephyr guidelines for multi-instance, multi-bus [Device Drivers].

It's automatically enabled when the devicetree contains compatible devices.

> [!NOTE]
>
> This driver *routes* all I2C/SPI communications through [System Calls]: the BME68X Sensor API is thus accessible from [User Mode] with the conventional restrictions.

[Device Drivers]: https://docs.zephyrproject.org/latest/kernel/drivers/index.html
[User Mode]: https://docs.zephyrproject.org/latest/kernel/usermode/index.html
[System Calls]: https://docs.zephyrproject.org/latest/kernel/usermode/syscalls.html


## Compatible Devices

BME680/688 devices are represented by [Devicetree] nodes with compatible string `bosch,bme68x-sensor-api`.

> [!NOTE]
>
> The use of "bosch" as vendor in this compatible string implies in no way that Bosch Sensortec endorses or promote this software: Bosch Sensortec is the vendor of the BME680/688 devices, not of this driver.
> See [Rules for vendor prefixes].

[Rules for vendor prefixes]: https://docs.zephyrproject.org/latest/build/dts/bindings-upstream.html#rules-for-vendor-prefixes

If in doubt, refer to the [Devicetree HOWTOs].

[Devicetree HOWTOs]: https://docs.zephyrproject.org/latest/build/dts/howtos.html
[Devicetree]: https://docs.zephyrproject.org/latest/build/dts/intro.html
[Sensors API]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor.html#sensors
[Device Drivers]: https://docs.zephyrproject.org/latest/kernel/drivers/index.html
[Bindings]: https://docs.zephyrproject.org/latest/build/dts/bindings.html

### I2C

[Bindings] for BME680/688 sensors connected to [I2C buses] are defined in [`bosch,bme68x-sensor-api-i2c.yaml`].

Their communication interface is defined by the Zephyr base bindings for I2C devices ([`i2c-device.yaml`]), and the only required property is the device address on the bus (`reg`), e.g.:

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

[`bosch,bme68x-sensor-api-i2c.yaml`]: /dts/bindings/bosch,bme68x-sensor-api-i2c.yaml
[`i2c-device.yaml`]: https://github.com/zephyrproject-rtos/zephyr/tree/main/dts/bindings/i2c/i2c-device.yaml
[I2C buses]: https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html

### SPI

[Bindings] for BME680/688 sensors connected to [SPI buses] are defined in [`bosch,bme68x-sensor-api-spi.yaml`].

Their communication interface is defined by the Zephyr base bindings for SPI devices ([`spi-device.yaml`]).

SPI interfaces configuration requires to set at least the maximum SPI clock frequency of the device:

``` dts
/* SPI bus the BME680/688 is connected to. */
&spi3 {
    bme680@0 {
        compatible = "bosch,bme68x-sensor-api";
        reg = < 0 >;

        /* BME680/688 device maximum clock frequency (10 MHz) */
        spi-max-frequency = < 10000000 >;

        /*
         * Default: duplex = < 0 > == < SPI_FULL_DUPLEX >;
         */

        /*
         * Default: frame-format = < 0 > == < SPI_FRAME_FORMAT_MOTOROLA >;
         */

        /*
         * BME680/688 SPI interface supports SPI mode '00' (CPOL = CPHA = '0')
         * and mode '11' (CPOL = CPHA = '1').
         *
         * If not set: '00'.
         */
        spi-cpol;
        spi-cpha;
    };
};
```

[`bosch,bme68x-sensor-api-spi.yaml`]: /dts/bindings/bosch,bme68x-sensor-api-spi.yaml
[`spi-device.yaml`]: https://github.com/zephyrproject-rtos/zephyr/tree/main/dts/bindings/spi/spi-device.yaml
[SPI buses]: https://docs.zephyrproject.org/latest/hardware/peripherals/spi.html

## Kconfig

Software configuration is done with [Kconfig].

| [`Kconfig`](Kconfig)                           | Configuration                                              |
|------------------------------------------------|------------------------------------------------------------|
| `BME68X_SENSOR_API_DRIVER`                     | Enable BME68X Sensor API (Driver)                          |
| `BME68X_SENSOR_API_DRIVER_INIT_PRIORITY (=99)` | Relative initialization priority ([Initialization Levels]) |
| `BME68X_SENSOR_API_DRIVER_LOG_LEVEL`           | Maximum log level                                          |

[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html
[Initialization Levels]: https://docs.zephyrproject.org/latest/kernel/drivers/index.html#initialization-levels

Configuration options are also accessible via the Kconfig menu: `Modules → bme68x → [*] BME68X Sensor API (Driver)`

> [!TIP]
>
> This driver and the BME68X Sensor API are automatically enabled when a device with "bosch,bme68x-sensor-api" bindings is connected to the devicetree.
