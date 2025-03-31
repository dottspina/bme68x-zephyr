# BME68X Sample ─ Environmental Sensing Service with BSEC and the BME68X Sensor API.

Adds the Bluetooth Environmental Sensing Service (ESS) provided by [lib/bme68x-esp] to the [samples/bme68x-iaq] application:

- initialize BME680/688 sensor device
- initialize and configure [BSEC] algorithm for IAQ
- enter BSEC control loop, logging *received* IAQ output samples
- expose Temperature, Pressure and Humidity as ESS characteristics

[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/
[samples/bme68x-iaq]: /samples/bme68x-iaq
[lib/bme68x-esp]: /lib/bme68x-esp

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

### BSEC

Refer to [lib/bme68x-iaq].

[lib/bme68x-iaq]: /lib/bme68x-iaq

### Environmental Sensor

To configure the Environmental Sensor role of the Environmental Sensing Profile:

- all configuration options are accessible via the Kconfig menu: `Modules → bme68x → [*] Environmental Sensing Profile`
- more generally, refer to [lib/bme68x-esp]

## Building and running

Nothing unusual, e:g:

```
$ cd bme68x-zephyr
$ west build samples/bme68x-esp -t menuconfig
$ west flash
```

> [!TIP]
>
> If the `bme68x` module is not installed as an external project managed by West, its path must be appended to `ZEPHYR_EXTRA_MODULES`:
>
> - in [`CMakeLists.txt`]
> - or as CMake variable, e.g. `west build -- -DZEPHYR_EXTRA_MODULES=/path/to/bme68x-zephyr`

[`CMakeLists.txt`]: CMakeLists.txt
