# BME68X Sample ─ Environmental Sensing Service (ESS)

Example use of the [lib/bme68x-esp] library: adds support for [Environmental Sensing Service] to [samples/bme68x-iaq], making [BSEC] output for Temperature, Pressure and Humidity available over Bluetooth LE.

A client application like [nRF Connect] then allows you to consult the measurements and configure notifications.

[lib/bme68x-esp]: /lib/bme68x-esp
[Environmental Sensing Service]: https://www.bluetooth.com/specifications/specs/environmental-sensing-service-1-0/
[samples/bme68x-iaq]: /samples/bme68x-iaq
[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/
[nRF Connect]: https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp

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

The default behavior is:
- to notify measurements to subscribed clients "When value changes compared to previous value"
- to accept only one connected client at a time, and automatically resume advertising, allowing re-connections
- to not authorize client applications to reconfigure notifications (read-only ES Trigger Setting descriptors)

> [!TIP]
>
> If `BME68X_ES_TRIGGER_SETTINGS_WRITE_AUHENT` is set, allowing clients to reconfigure notifications only over authenticated connections, the application will automatically register a simple logging-based *DisplayOnly* callbacks.

## Building and running

Nothing unusual, e.g.:

```
$ cd bme68x-zephyr
$ west build samples/bme68x-esp -t menuconfig
$ west flash
```
