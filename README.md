# bme68x-zephyr ─ Alternative BME680/688 sensors support for Zephyr-RTOS

Device driver, support libraries and sample applications for Bosch Sensortec's [BME68X Sensor API] and [BSEC] integration with Zephyr-RTOS.

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/

| Library                     | Provides                                                    |
|-----------------------------|-------------------------------------------------------------|
| [lib/bme68x-sensor-api]     | Bosch Sensortec's BME68X Sensor API                         |
| [drivers/bme68x-sensor-api] | BME68X Sensor API integration with Zephyr-RTOS (driver)     |
| [lib/bsec]                  | Bosch Sensortec Environmental Cluster (BSEC)                |
| [lib/bme68x-iaq]            | Support library for IAQ with the BME68X Sensor API and BSEC |

[lib/bme68x-sensor-api]: lib/bme68x-sensor-api
[drivers/bme68x-sensor-api]: drivers/bme68x-sensor-api
[lib/bsec]: lib/bsec
[lib/bme68x-iaq]: lib/bme68x-iaq

| Sample                | Application                                                     |
|-----------------------|-----------------------------------------------------------------|
| [samples/bme68x-tphg] | Forced THPG measurements with the BME68X Sensor API             |
| [samples/bme68x-iaq]  | Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API |

[samples/bme68x-tphg]: samples/bme68x-tphg
[samples/bme68x-iaq]: samples/bme68x-iaq

> [!IMPORTANT]
>
> For simple periodic measurements of temperature, pressure, humidity and gas resistance, the upstream [BME680 driver] SHOULD be preferred.
>
> This alternative support may be helpful as a starting point:
>
> - when needing complete control over BME680/688 devices with the BME68X Sensor API
> - in particular for integration with the Bosch Sensortec Environmental Cluster (BSEC) library
>
> Repository content:
>
> - a couples of years ago, I wrote a simple Zephyr application with Index for Air Quality (IAQ) support for my new [BME680] sensor
> - I recently resurrected the BME680, and realized that I had thrown away all the *boiler plate* code and configuration files needed for integration with the Zephyr build system, Devicetree and device drivers
> - this repository will help me keep something almost clean that should work *Out of the box* in case I want to play with my BME680 again
> - it's made public as an installable Zephyr module so as not to reinvent the wheel, or simply as a source of inspiration to reinvent your own wheel
> - Apache-2.0 license, on an "AS IS" BASIS WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND
> - [Rationale](#rationale) details why the upstream BME680 driver cannot be suitable for this kind of use cases
>
> Browsing through the [Quick Start Guide](#quick-start-guide), which describes how to get the IAQ application sample up and running *in five minutes*, may also help to get the big picture.

[BME680 driver]: https://docs.zephyrproject.org/latest/samples/sensor/bme680/README.html
[BME680]: https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/
[BME680 Datasheet]: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf


## Installation

The driver and support libraries are provided by the [Zephyr Module] `bme68x`, which can be *installed*:

- either as an *external project* managed by West
- or as an *extra module* given to CMake

[Zephyr Module]: https://docs.zephyrproject.org/latest/develop/modules.html

### External project

We assume a Zephyr command line development environment managed by [West] ([Workspaces]), installed e.g. following the Zephyr [Getting Started Guide]:

```
zephyr-project/
├── bootloader/
│   └── mcuboot/
├── modules/
│   ├── bsim_hw_models/
│   ├── ... stripped ...
│   ├── lib
│   └── tee
├── tools/
└── zephyr/
    ├── arch/
    ├── ... stripped ...
    └── west.yml
```

Add an external project entry at the end of the workspace [manifest] (`zephyr-project/zephyr/west.yml`), e.g.:

``` yml
projects:
    # ... stripped ...
    - name: bme68x
      url: https://github.com/dottspina/bme68x-zephyr.git
      revision: main
      path: modules/lib/bme68x
```

Then update the defined projects:

```
$ west update
=== updating bme68x (modules/lib/bme68x):
--- bme68x: initializing
Initialized empty Git repository in /path/to/zephyr-project/modules/lib/bme68x/.git/
--- bme68x: fetching, need revision main
remote: Enumerating objects: 230, done.
remote: Counting objects: 100% (230/230), done.
remote: Compressing objects: 100% (148/148), done.
remote: Total 230 (delta 97), reused 207 (delta 74), pack-reused 0
Receiving objects: 100% (230/230), 118.89 KiB | 637.00 KiB/s, done.
Resolving deltas: 100% (97/97), done.
From github.com:dottspina/bme68x-zephyr
 * branch            main       -> FETCH_HEAD
```

The module `bme68x` should now be installed in `zephyr-project/modules/lib/bme68x`.

> [!TIP]
>
> This is the recommended approach:
>
> - when planning to use the Bosch Sensortec Environmental Cluster (BSEC) library: it permits to simply install the required binaries as [Zephyr binary blobs]
> - more generally, this vastly simplifies the build-system configuration of depending applications

[West]: https://docs.zephyrproject.org/latest/develop/west/index.html
[Workspaces]: https://docs.zephyrproject.org/latest/develop/west/workspaces.html
[Getting Started Guide]: https://docs.zephyrproject.org/latest/develop/getting_started/index.html
[manifest]: https://docs.zephyrproject.org/latest/develop/west/manifest.html
[Zephyr binary blobs]: https://docs.zephyrproject.org/latest/contribute/bin_blobs.html

### Extra module

For use in applications without messing with manifest files or workspaces, clone the project and add the extra module to the build system, typically in your `CMakeList.txt` (**before** `find_package()`):

``` cmake
list(APPEND ZEPHYR_EXTRA_MODULES
   # Absolute path to the bme68x module (the directory that contains zephyr/module.yml).
   /path/to/bme68x-zephyr
)
```

> [!TIP]
>
> If building a sample application, enable the corresponding lines in its `CMakeList.txt`:
>
> ``` cmake
> list(APPEND ZEPHYR_EXTRA_MODULES
>     ${CMAKE_SOURCE_DIR}/../..
> )
> ```

Depending on your workflow and environment, this approach may also require additional configuration of the [Zephyr Build system]:

- to properly build the additional [System Calls] defined by this module: search the [Zephyr documentation] for `SYSCALL_INCLUDE_DIRS`
- to add the [Devicetree Bindings] for the `bme68x-sensor-api` driver: see e.g. [Using an external DTS binding for an out-of-tree driver] or search the Zephyr documentation for `DTS_ROOT`

It will also require manual installation of the BSEC algorithm binaries: see [lib/bsec]

[Integrate modules in Zephyr build system] may also prove helpful.

[Zephyr Build System]: https://docs.zephyrproject.org/latest/build/cmake/index.html
[System Calls]: https://docs.zephyrproject.org/latest/kernel/usermode/syscalls.html
[Zephyr documentation]: https://docs.zephyrproject.org/latest/index.html
[Integrate modules in Zephyr build system]: https://docs.zephyrproject.org/latest/develop/modules.html#integrate-modules-in-zephyr-build-system
[Devicetree Bindings]: https://docs.zephyrproject.org/latest/build/dts/bindings.html
[Using an external DTS binding for an out-of-tree driver]: https://lists.zephyrproject.org/g/users/topic/using_an_external_dts_binding/78139373

### Configuration

Libraries configuration is carried out with [Kconfig]. When using an interactive interface, e.g. `west build -t menuconfig`, the main menu is `Modules → bme68x`.

[Kconfig]:https://docs.zephyrproject.org/latest/build/kconfig/index.html

Top-level configuration options are summarized bellow.

| Library                     | Kconfig                    | Configuration                            |
|-----------------------------|----------------------------|------------------------------------------|
| [lib/bme68x-sensor-api]     | `BME68X_SENSOR_API`        | Enable BME68X Sensor API                 |
|                             | `BME68X_SENSOR_API_FLOAT`  | Select floating-point variant of the API |
| [drivers/bme68x-sensor-api] | `BME68X_SENSOR_API_DRIVER` | Enable BME68X Sensor API (driver)        |
| [lib/bsec]                  | `BSEC`                     | Enable BSEC library                      |
| [lib/bme68x-iaq]            | `BME68X_IAQ`               | Enable support library for BSEC IAQ      |
|                             | `BME68X_IAQ_SETTINGS`      | Enable BSEC state persistence            |

> [!TIP]
>
> `BME68X_SENSOR_API_DRIVER` and `BME68X_SENSOR_API` are implied when an application devicetree contains compatible devices.

### Sample applications

The necessary steps for building and running sample applications are nothing unusual:

- *connect* sensors to the [Devicetree], typically with [devicetree overlays]
- review the Kconfig options that configure the application and the libraries it depends on; `west build -t menuconfig` is handy
- build and run, e.g.:

```
$ cd bme68x-zephyr
$ west build samples/bme68x-iaq
$ west flash
```

Each application contains example configuration and overlay files for [nRF52840 DK]. Configuration for your board will probably differ, refer to Zephyr [Supported Boards] and your SoC documentation.

> [!TIP]
>
> Consider trying the [samples/bme68x-tphg] application first to test your [communication interface] configuration.

[Devicetree]: https://docs.zephyrproject.org/latest/build/dts/intro.html
[devicetree overlays]: https://docs.zephyrproject.org/latest/build/dts/howtos.html#set-devicetree-overlays
[nRF52840 DK]: https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html
[Supported Boards]: https://docs.zephyrproject.org/latest/boards/index.html
[communication interface]: /drivers/bme68x-sensor-api/README.md#compatible-devices

## Quick Start Guide

Assuming this `bme68x` module has been [installed as an external project](#external-project), and depending on your board, this brief guide may be sufficient to get the Index for Air Quality (IAQ) sample application ([samples/bme68x-iaq]) up and running *in five minutes*:

- verify installation
- connect a BME680/688 sensor to the devicetree (I2C)
- configure the BSEC algorithm
- build and run the application

The application contains DTS overlay and configuration files for [nRF52840 DK] in [its boards directory].

[its boards directory]: samples/bme68x-iaq/boards

### Verify installation

This sample application depends on the BSEC library. If not already installed:

- accept BSEC [License Terms and Conditions]
- install BSEC static libraries with the [`west blobs`] command

```
$ west blobs fetch bme68x
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m3/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m33/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m33f/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m4/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m4f/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/esp32/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/esp32s2/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/esp32s3/libalgobsec.a
Fetching blob bme68x: /path/to/zephyr-project/modules/lib/bme68x/zephyr/blobs/bsec/esp32c3/libalgobsec.a
```

[`west blobs`]: https://docs.zephyrproject.org/latest/develop/west/zephyr-cmds.html#west-blobs
[License Terms and Conditions]: https://www.bosch-sensortec.com/media/boschsensortec/downloads/software/bme688_development_software/2023_04/license_terms_bme688_bme680_bsec.pdf

### Connect sensor

Connect the BME680/688 sensor by adding a DTS overlay file to the `bme68x-zephyr/samples/bme68x-iaq/boards` directory.

For example, with:

- an [Adafruit Feather nRF52840 Express] board
- a BME680/688 sensor connected to I2C, with slave address `0x77`

We would add an `adafruit_feather_nrf52840.overlay` file with the content bellow:

``` dts
&i2c0 {
    compatible = "nordic,nrf-twi";
    bme680: bme680@77 {
        compatible = "bosch,bme68x-sensor-api";
        reg = <0x77>;
    };
};
```

Refer to [drivers/bme68x-sensor-api] for more detail, especially for connecting SPI devices.

[Adafruit Feather nRF52840 Express]: https://docs.zephyrproject.org/latest/boards/adafruit/feather/doc/index.html

### Configure BSEC

BSEC algorithm configuration includes:

- BME680/688 sensor's supply voltage: 3.3V or 1.8V
- desired BSEC calibration time: 4 or 28 days
- BSEC sample rate: Low-Power (LP, 1/3 Hz) or Ultra-Low-Power (ULP, 1/300 Hz)

These options are accessible via the Kconfig menu: `Modules → bme68x → [*] Support library for BSEC IAQ → IAQ configuration`.

> [!TIP]
>
> The default configuration is 3.3V, 4 days, LP mode:
> - check that the voltage that your board supplies to the sensor is 3.3 V
> - if so, simply continue with this configuration

### Build and run

The application should build and run *out of the box* for the target CPU families bellow:

- Cortex-M: M33, M33F, M4, M4F
- ESP32: ESP32, ESP32S2, ESP32S3, ESP32C3

```
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
[00:00:00.638,549] <inf> app: -- IAQ output signals (13) --
[00:00:00.638,549] <inf> app: T:26.68 degC
[00:00:00.638,580] <inf> app: P:101.092 kPa
[00:00:00.638,580] <inf> app: H:67.14 %
[00:00:00.638,580] <inf> app: G:37.656 kOhm
[00:00:00.638,610] <inf> app: IAQ:58 (unreliable)
[00:00:00.638,641] <inf> app: CO2:562 ppm (unreliable)
[00:00:00.638,671] <inf> app: VOC:0.57 ppm (unreliable)
[00:00:00.638,702] <inf> app: stabilization: finished, on-going
...
[00:10:00.553,863] <inf> app: -- IAQ output signals (13) --
[00:10:00.553,863] <inf> app: T:26.73 degC
[00:10:00.553,863] <inf> app: P:101.143 kPa
[00:10:00.553,894] <inf> app: H:67.00 %
[00:10:00.553,894] <inf> app: G:35.182 kOhm
[00:10:00.553,924] <inf> app: IAQ:71 (high accuracy)
[00:10:00.553,955] <inf> app: CO2:663 ppm (high accuracy)
[00:10:00.553,985] <inf> app: VOC:0.51 ppm (high accuracy)
[00:10:00.554,016] <inf> app: stabilization: finished, finished
```

For other CPUs, or if the build fails with some "Failed to guess BSEC target" or "BSEC blob not found" message, refer to [lib/bsec].

Once this works, refer to [lib/bme68x-iaq] to enable and configure BSEC state persistence to the per-device settings (Zephyr [Settings] subsystem).

> [!TIP]
>
> Boards that include some Flash memory will likely support NVS and define a "storage" partition in their DTS.
> Enabling BSEC state persistence can then be as simple as:
>
> ``` conf
> # Enable the Settings subsystem (NVS backend)
> CONFIG_NVS=y
> CONFIG_FLASH=y
> CONFIG_FLASH_MAP=y
> CONFIG_SETTINGS=y
> CONFIG_SETTINGS_NVS=y
>
> # Enable BSEC state persistence to per-device settings
> CONFIG_BME68X_IAQ_SETTINGS=y
> ```

[Settings]: https://docs.zephyrproject.org/latest/services/storage/settings/

## Rationale

Bosch Sensortec's BME680/688 devices are *sensors* for measuring ambient temperature, barometric pressure, relative humidity, and gas resistance (TPHG).

Their simplest use is what is referred to as *forced measurements with single heating profile* in Low Power (LP) or Ultra-Low Power (ULP) mode.

This is the behavior the upstream [BME680 driver] makes available through the Zephyr [Sensors API]:

- the BME680 sensor is configured and the first measurement triggered during driver initialization
- the application then uses [`sensor_sample_fetch()`] to read the result of the last measurement and trigger a new one
- temperature, pressure, humidity and gas resistance are available as [sensor values] with [`sensor_channel_get()`]

This approach has advantages:

- the application can get TPHG measurements without any prior sensor initialization or configuration step
- integration with Zephyr [Sensing] subsystem for applications that rely on this service for combining sensors

But it also presents limitations that are difficult, if not impossible, to resolve when requiring finer control of sensors:

- the application can't switch the device to the *parallel* or *sequential* modes supported by BME688 sensors
- the application can't reconfigure sensors, nor use more than one heating profile
- timestamp is confusing: measurements are triggered when `sensor_sample_fetch()` is called, but the values ​​you can then access with `sensor_channel_get()` date from the *previous* measurement, not the one you just requested

[BME680 driver]: https://docs.zephyrproject.org/latest/samples/sensor/bme680/README.html
[Sensors API]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor.html
[Sensing]: https://docs.zephyrproject.org/latest/services/sensing/index.html
[sensor values]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor.html#c.sensor_value
[`sensor_sample_fetch()`]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html#c.sensor_sample_fetch
[`sensor_channel_get()`]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html#c.sensor_channel_get

Examples that immediately come to mind include:

- gas scanner support for [BME688] ([How to distinguish BME680 from BME688 in firmware])
- integration with Bosch Sensortec Environmental Cluster ([BSEC]) for Index for Air Quality (IAQ) and gas detection
- user initiated sensor configuration (e.g. with a Bluetooth smartphone)

My personal use case was simply to get IAQ estimates, out of curiosity: to do this, I first had to *reinvent the wheel*, from the Zephyr device driver layer to the BSEC library integration. Only after that I could consider writing a simple PoC application.

This isn't *rocket science*, but it could discourage Zephyr enthusiasts who have just received their new BME680/688, or direct them toward proprietary solutions:

- [BME68X Sensor API running on Zephyr RTOS] (Bosch Sensortec Community)
- [BME680 + NRF9160 in ZephyrRTOS] (Bosch Sensortec Community)
- nRF Connect SDK [BME68X IAQ driver]

[How to distinguish BME680 from BME688 in firmware]: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/How-to-distinguish-BME680-from-BME688-in-firmware/td-p/73929
[BME688]: https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme688/
[BME68X Sensor API running on Zephyr RTOS]: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/BME68X-Sensor-API-running-on-Zephyr-RTOS/m-p/92579
[BME680 + NRF9160 in ZephyrRTOS]: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/BME680-NRF9160-in-ZephyrRTOS/m-p/23571
[BME68X IAQ driver]: https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/drivers/bme68x_iaq.html

Future work: implement a driver that supports both the Zephyr Sensor API and BSEC IAQ mode ?
