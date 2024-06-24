# BME68X Sample ─ Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API

Sample application for Index for Air Quality with Bosch Sensortec Environmental Cluster ([BSEC]) and the [BME68X Sensor API]:

- initialize BME680/688 sensor device
- initialize and configure BSEC algorithm for IAQ
- enter BSEC control loop, logging *received* IAQ output samples

[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/
[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI

## Configuration

The [boards](boards) directory contains example DTS overlay and configuration files for [nRF52840 DK].

See also [`prj.conf`](prj.conf) and the [Kconfig] menus:

- `Modules → bme68x → [*] Support library for BSEC IAQ`
- `BME68X Sample - IAQ`

[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html
[nRF52840 DK]: https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html

### Communication interface

This sample application relies on [drivers/bme68x-sensor-api] to communicate with the controlled BME680/688 device (I2C/SPI). The communication interface must be described in the devicetree, typically with an overlay file, e.g.:

``` dts
/* I2C bus the BME680/688 device is connected to. */
&i2c0 {
    bme680@76 {
        compatible = "bosch,bme68x-api";

        /* BME68X I2C slave address (0x76 or 0x77) */
        reg = < 0x76 >;
    };
};
```

> [!TIP]
>
> Consider trying the simpler [samples/bme68x-tphg] application first to test [communication interface] configurations.

[drivers/bme68x-sensor-api]: /drivers/bme68x-sensor-api
[samples/bme68x-tphg]: /samples/bme68x-tphg
[communication interface]: /drivers/bme68x-sensor-api/README.md#compatible-devices


### BSEC

Refer to [lib/bme68x-iaq] to configure the BSEC algorithm and state persistence to flash storage ([NVS]).

[lib/bme68x-iaq]: /lib/bme68x-iaq
[NVS]: https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html


## Building and running

Nothing unusual, e.g.:

``` sh
$ cd bme68x-zephyr
$ west build samples/bme68x-iaq -t menuconfig
$ west flash
```

Console output:

```
*** Booting Zephyr OS build v3.6.0 ***
[00:00:00.359,252] <inf> bme68x_sensor_api: bme680@77 (Fixed-point API)
[00:00:00.377,075] <inf> bme68x_iaq: BSEC 2.5.0.2
[00:00:00.379,333] <inf> bme68x_iaq: loaded BSEC configuration (2063 bytes)
[00:00:00.384,613] <inf> fs_nvs: 2 Sectors of 4096 bytes
[00:00:00.384,613] <inf> fs_nvs: alloc wra: 1, fb8
[00:00:00.384,613] <inf> fs_nvs: data wra: 1, 464
[00:00:00.384,643] <inf> bme68x_iaq: NVS-FS at 0xfe000 (2 x 4096 bytes)
[00:00:00.384,643] <inf> bme68x_iaq: BSEC state save period: 10 min
[00:00:00.384,887] <inf> bme68x_iaq: loaded BSEC state (221 bytes)
[00:00:00.386,108] <inf> bme68x_iaq: BSEC subscriptions: 13/8
[00:00:00.390,625] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: os_t:2 os_p:5 os_h:1
[00:00:00.393,585] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: heatr_temp(degC):320 heatr_dur(ms):197
[00:00:00.394,653] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: forced mode
[00:00:00.394,653] <dbg> bme68x_iaq: bme68x_iaq_run: TPHG wait: 239590 us ...
[00:00:00.638,549] <inf> app: -- IAQ output signals (13) --
[00:00:00.638,549] <inf> app: T:26.68 degC
[00:00:00.638,580] <inf> app: P:101.092 kPa
[00:00:00.638,580] <inf> app: H:67.14 %
[00:00:00.638,580] <inf> app: G:37.656 kOhm
[00:00:00.638,610] <inf> app: IAQ:58 (unreliable)
[00:00:00.638,641] <inf> app: CO2:562 ppm (unreliable)
[00:00:00.638,671] <inf> app: VOC:0.57 ppm (unreliable)
[00:00:00.638,702] <inf> app: stabilization: finished, on-going
[00:00:00.638,732] <dbg> bme68x_iaq: bme68x_iaq_run: BSEC wait: 2747375 us ...
[00:00:03.389,984] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: os_t:2 os_p:5 os_h:1
[00:00:03.393,127] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: heatr_temp(degC):320 heatr_dur(ms):197
[00:00:03.394,256] <dbg> bme68x_iaq: iaq_bsec_trigger_measurement: forced mode
[00:00:03.394,287] <dbg> bme68x_iaq: bme68x_iaq_run: TPHG wait: 239590 us ...
[00:00:03.638,153] <inf> app: -- IAQ output signals (13) --
[00:00:03.638,153] <inf> app: T:26.70 degC
[00:00:03.638,153] <inf> app: P:101.094 kPa
[00:00:03.638,183] <inf> app: H:67.20 %
[00:00:03.638,183] <inf> app: G:35.152 kOhm
[00:00:03.638,214] <inf> app: IAQ:61 (unreliable)
[00:00:03.638,244] <inf> app: CO2:589 ppm (unreliable)
[00:00:03.638,275] <inf> app: VOC:0.61 ppm (unreliable)
[00:00:03.638,305] <inf> app: stabilization: finished, on-going
...
[00:10:00.553,863] <inf> app: -- IAQ output signals (13) --
[00:10:00.553,863] <inf> app: T:26.73 degC
[00:10:00.553,863] <inf> app: P:101.143 kPa
[00:10:00.553,894] <inf> app: H:67.00 %
[00:10:00.553,894] <inf> app: G:35.182 kOhm
[00:10:00.553,924] <inf> app: IAQ:71 (high accuracy)
[00:10:00.553,955] <inf> app: CO2:663 ppm (high accuracy)
[00:10:00.553,985] <inf> app: VOC:0.72 ppm (high accuracy)
[00:10:00.554,016] <inf> app: stabilization: finished, finished
[00:10:00.556,823] <inf> bme68x_iaq: saved BSEC state (221 bytes)
```

> [!TIP]
>
> If the `bme68x` module is not installed as an external project managed by West, its path must be appended to `ZEPHYR_EXTRA_MODULES`:
>
> - in [`CMakeLists.txt`]
> - or as CMake variable, e.g. `west build -- -DZEPHYR_EXTRA_MODULES=/path/to/bme68x-zephyr`

[`CMakeLists.txt`]: CMakeLists.txt
