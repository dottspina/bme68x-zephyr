# bme68x-zephyr ─ Alternative BME680/688 sensors support for Zephyr-RTOS

Alternative device driver for BME680/688 sensors (I2C/SPI) and support libraries for Bosch Sensortec's [BME68X Sensor API] and [BSEC] integration with Zephyr-RTOS.

Since watching the measurements on the application log is not really fun, this module also includes an experimental implementation of the Bluetooth [Environmental Sensing Service]: it currently supports the Temperature, Pressure and Humidity ESS Characteristics and all variants of ES Trigger Setting descriptors. It does not depend on BME680/688 sensors or BSEC, and can be tried independently.

| Library                     | Provides                                                          |
|-----------------------------|-------------------------------------------------------------------|
| [lib/bme68x-sensor-api]     | Bosch Sensortec's BME68X Sensor API                               |
| [drivers/bme68x-sensor-api] | Alternative BME68X I2C/SPI drivers                                |
| [lib/bsec]                  | Bosch Sensortec Environmental Cluster (BSEC) library              |
| [lib/bme68x-iaq]            | Support library for IAQ with the BME68X Sensor API and BSEC       |
| [lib/bme68x-esp]            | Support library for the Bluetooth [Environmental Sensing Profile] |

Various sample applications are available.

| Sample                | Application                                         |
|-----------------------|-----------------------------------------------------|
| [samples/bme68x-tphg] | Forced THPG measurements with the BME68X Sensor API |
| [samples/bme68x-iaq]  | Index for Air Quality (IAQ) with BSEC               |
| [samples/bme68x-esp]  | Environmental Sensing Service with BSEC             |

> [!IMPORTANT]
>
> For simple measurements of temperature, pressure, humidity and gas resistance,
> Zephyr provides the [`bosch,bme680`] device driver which implements the [Fetch and Get] subset
> of the [Sensors] API: check it first.
>
> This alternative support may be helpful as a starting point:
>
> - when instead planning to access the BME680/688 devices through the *standard* BME68X Sensor API, e.g. for features specific to the BME688
> - when needing complete control over the BME680/688 sensors configuration, e.g. to implement the periodic reconfiguration required for integration with Bosch Sensortec Environmental Cluster (BSEC)
>
> Those interested in the limitations of the upstream BME680 driver that this alternative attempts to address can also take a look at [Rationale](#rationale).
>
> **About this repository**:
>
> - years ago, I wrote a simple Zephyr application with Index for Air Quality (IAQ) support for my new [BME680] sensor
> - then, one day, I resurrected the BME680, and realized that I had thrown away the whole thing, including all the *boiler plate* code and configuration files needed for integration with the Zephyr build system, Devicetree and device drivers
> - this repository will help me keep something almost clean that should work *out of the box* when I want to play with the BME680
> - it's made public as a Zephyr module so others don't have to reinvent the wheel, or simply as a source of inspiration to reinvent their own wheel
> - Apache-2.0 license, on an "AS IS" BASIS WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND
> - Recently tested with Zephyr 4.3.0

Skimming through the [Quick Start Guide](#quick-start-guide), which describes how to get the IAQ application sample up and running *in five minutes*, may help to get the big picture.

## Installation

The driver and support libraries are provided as a [Zephyr Module] (`bme68x`), which can be *installed*:

- either as an *external project* managed by West (recommended)
- or as an *extra module* given to CMake

### External project

This method assumes a Zephyr command line development environment managed by [West].

To install this `bme68x` module as an external project, simply create a new [manifest] file in your `zephyr/submanifests` directory, e.g. `bme68x.yaml`, with the content bellow:

``` yaml
manifest:
  projects:
    - name: bme68x
      url: https://github.com/dottspina/bme68x-zephyr.git
      revision: main
      path: modules/lib/bme68x
```

Then fetch (or update) the external project as usual: `west update bme68x`

Bosch Sensortec provides the BSEC library as binary blobs for various hardware and toolchains. These binaries are proprietary and we can't redistribute them.
However, the project manifest includes definitions ([Zephyr binary blobs]) for ELF static libraries targeting most of the supported Cortex-M and ESP32 microcontrollers, which you can install at any time:

- Accept [BSEC License Terms and Conditions]
- Download binary blobs `west blobs fetch bme68x`

See [lib/bsec] for more details, especially if your MCU family is not predefined or recognized.

The changes made to the workspace are summarized bellow:

```
zephyr-project/
├── modules/
│   └── lib/
│       └── bme68x/
│           ├── CMakeLists.txt
│           ├── drivers/
│           ├── dts/
│           ├── Kconfig
│           ├── lib/
│           ├── samples/
│           └── zephyr/
|               └── blobs/
|                   └── bsec/
|                       └── <MCU>/
|                           └── libalgobsec.a
└── zephyr/
    └── submanifests/
        └── bme68x.yaml
```

> [!TIP]
>
> This is the recommended approach, which greatly simplifies:
> - the installation and integration of the BSEC library
> - the build-system configuration for dependent applications

### Extra module

This approach does not depend on West and instead requires to *manually* configure the build system of each dependent project.

Most of the time it's enough to simply add the local path of your clone (or fork) of this module repository to the extra modules the project depends on.
For example, in your top-level `CMakeList.txt` (**before** `find_package(ZEPHYR)`):

``` cmake
list(APPEND ZEPHYR_EXTRA_MODULES
   # Absolute path to your clone of the bme68x-zephyr module repository.
   /path/to/bme68x-zephyr
)
```

Bosch Sensortec provides the BSEC library as binary blobs for various hardware and toolchains. These binaries are proprietary and we can't redistribute them.
Unfortunately, this approach does not come with a semi-automatic method for downloading them to their appropriate location. See [lib/bsec] for more details.

> [!NOTE]
>
> Very specific workflows or environments might require additional configuration steps:
>
> - to properly build the [System Calls] defined by this module: search the [Zephyr documentation] for `SYSCALL_INCLUDE_DIRS`
> - to add the [Devicetree Bindings] for this `bosch,bme68x-sensor-api` driver: see e.g. [Using an external DTS binding for an out-of-tree driver] or search the Zephyr documentation for `DTS_ROOT`
>
> [Integrate modules in Zephyr build system] may also help.

## Configuration

As expected when developing with Zephyr:

- [Kconfig] is used to configure libraries and applications
- [Devicetree] is used to set up the device driver and connect hardware

### Device driver

| Library                     | Kconfig                    | Enable                                |
|-----------------------------|----------------------------|---------------------------------------|
| [lib/bme68x-sensor-api]     | `BME68X_SENSOR_API`        | Bosch Sensortec's [BME68X Sensor API] |
| [drivers/bme68x-sensor-api] | `BME68X_SENSOR_API_DRIVER` | `bosch,bme68x-sensor-api` driver      |

This `bosch,bme68x-sensor-api` driver permits to initialize the BME68X Sensor API communication interface with a device retrieved from the devicetree:

``` c
/* BME68X Sensor API I2C/SPI communication interface. */
struct bme68x_dev bme68x_dev;
/* Retrieve I2C/SPI device from the devicetree. */
struct device const *dev = DEVICE_DT_GET_ONE(bosch_bme68x_sensor_api);
/* Integrate device driver with the BME68X Sensor API communication interface. */
bme68x_sensor_api_init(dev, &bme68x_dev);

/* From now use the BME68X Sensor API as usual,
 * starting with:
 */
bme68x_init(&bme68x_dev)
```

The DTS files simply define the BME680 and BME688 devices as sensors connected to I2C or SPI buses:

- I2C: [`bosch,bme68x-sensor-api-i2c.yaml`]
- SPI: [`bosch,bme68x-sensor-api-spi.yaml`]

Typically the connection takes place in some [devicetree overlay] file, e.g.:

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

> [!TIP]
>
> Driver (`BME68X_SENSOR_API_DRIVER`) and BME68X Sensor API (`BME68X_SENSOR_API`) are automatically enabled when a devicetree connects compatible devices, like in the example above.

### Libraries

| Library                 | Kconfig             | Enable                                        |
|-------------------------|---------------------|-----------------------------------------------|
| [lib/bsec]              | `BSEC`              | Bosch Sensortec Environmental Cluster         |
| [lib/bme68x-iaq]        | `BME68X_IAQ`        | Support library for BSEC IAQ                  |
| [lib/bme68x-esp]        | `BME68X_ESP`        | Bluetooth Environmental Sensing Service (ESS) |

The above libraries define a number of options: the easiest way to get an idea of ​​what is configurable is to browse their respective menus under `Modules → bme68x` using one of the various Kconfig user interfaces, e.g. `west build -t menuconfig`.

## Sample applications

| Sample                | Application                                         |
|-----------------------|-----------------------------------------------------|
| [samples/bme68x-tphg] | Forced THPG measurements with the BME68X Sensor API |
| [samples/bme68x-iaq]  | Index for Air Quality (IAQ) with BSEC               |
| [samples/bme68x-esp]  | Environmental Sensing Service with BSEC             |

Sample applications are included in the `samples` sub-directory of the module.

Each application contains example configuration and overlay files for [nRF52840 DK]. Configuration for your board will probably differ, refer to Zephyr [Supported Boards] and your SoC documentation.

> [!TIP]
>
> If not comfortable with Zephyr configuration or Devicetree, consider the simple [samples/bme68x-tphg] application first to test your [communication interface](drivers/bme68x-sensor-api/README.md#compatible-devices) to the BME680/688 sensor.

## Quick Start Guide

For simplicity, this brief guide assumes a development environment managed by [West],
installed as described in Zephyr [Getting Started Guide].
Depending on your board, it may be sufficient to get the Index for Air Quality (IAQ) sample application ([samples/bme68x-iaq]) up and running *in five minutes*:

- install this module
- connect the BME680/688 sensor
- configure the BSEC algorithm
- build and run the application

> [!TIP]
>
> Once this works, you can try the [samples/bme68x-esp] application to add BLE connectivity.

### Install this module

Assuming `zephyr-project` represents your West workspace root, and the environment is correctly initialized (Python virtual environment, `ZEPHYR_BASE`, `ZEPHYR_TOOLCHAIN_VARIANT`, etc):

1. copy this [bme68x.yaml](bme68x.yaml) manifest file to the `zephyr-project/zephyr/submanifests` directory
2. install the module: `west update bme68x`
3. accept [BSEC License Terms and Conditions] and install proprietary binaries: `west blobs fetch bme68x`

### Connect the sensor

Connect the BME680/688 sensor by adding a DTS overlay file to the `samples/bme68x-iaq/boards` directory.

For example, with:

- an [Adafruit Feather ESP32S2] board
- a BME680 sensor connected to I2C, with slave address `0x77`

you could add an `adafruit_feather_esp32s2.overlay` file with the content bellow:

``` dts
&i2c0 {
    bme680@77 {
        compatible = "bosch,bme68x-sensor-api";
        reg = <0x77>;
        power-domains = <&i2c_reg>;
    };
};
```

Refer to [drivers/bme68x-sensor-api] for details, especially for connecting SPI devices.

The application contains example DTS overlays in [its boards directory](samples/bme68x-iaq/boards).

### Configure BSEC

The BSEC algorithm configuration includes:

- supply voltage for the BME680/688 sensor: 3.3V or 1.8V
- desired BSEC calibration time: 4 or 28 days
- BSEC sample rate: Low-Power (LP, 1/3 Hz) or Ultra-Low-Power (ULP, 1/300 Hz)

Options for predefined IAQ configurations are accessible via Kconfig (e.g. menu `Modules → bme68x → [*] Support library for BSEC IAQ → IAQ configuration`).

> [!TIP]
>
> The default configuration is 3.3V, 4 days, LP mode:
> - check that the voltage that your board supplies to the sensor is 3.3 V
> - if so, simply continue with this configuration

### Build and run the application

The application should build *out of the box* for the MCU families bellow:

- Cortex-M: M33, M33F, M4, M4F
- ESP32: ESP32, ESP32S2, ESP32S3, ESP32C3

For other targets, or if the build fails with "Failed to guess BSEC target" or "BSEC blob not found", please refer to [lib/bsec].

Apart from that, build and run as usual, e.g.:

``` sh
cd zephyr-project/modules/lib/bme68x
west build samples/bme68x-iaq -b nrf52840dk/nrf52840
west flash
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

Once this works, you can try to enable and configure BSEC state persistence to per-device settings (Zephyr [Settings] subsystem). Please refer to [lib/bme68x-iaq].

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


## Rationale

Bosch Sensortec's BME680/688 devices are *sensors* for measuring ambient temperature, barometric pressure, relative humidity, and gas resistance (TPHG).

Their simplest use is what is referred to as *forced measurements with single heating profile* in Low Power (LP) or Ultra-Low Power (ULP) mode.

This is the use case the upstream `bosch,bme680` driver makes available:

- the BME680 sensor is configured for once during the driver initialization
- new measurements are triggered with `sensor_sample_fetch()`
- temperature, pressure, humidity and gas resistance are available with `sensor_channel_get()`

This approach has advantages: the application can get TPHG measurements without any additional initialization or configuration steps.

But it also presents limitations that are difficult, if not impossible, to resolve when requiring finer control of sensors:

- the application can't switch the device to the *parallel* mode supported by BME688 sensors
- the application can't reconfigure the device
- the application is limited to one heating profile

Examples that immediately come to mind include:

- gas scanner support for [BME688] ([How to distinguish BME680 from BME688 in firmware])
- integration with Bosch Sensortec Environmental Cluster (BSEC) for Index for Air Quality (IAQ) and gas detection
- user initiated sensor configuration (e.g. from a Bluetooth smartphone)

My personal use case was simply to get IAQ estimates, out of curiosity: to do this, I first had to *reinvent the wheel*, from the Zephyr device driver layer to the BSEC library integration. Only after that I could consider writing a simple PoC application.

This isn't *rocket science*, but it could discourage Zephyr enthusiasts who have just received their new BME680/688, or direct them toward proprietary solutions like nRF Connect SDK [BME68X IAQ driver].

[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI
[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/
[BME680]: https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/
[Environmental Sensing Service]: https://www.bluetooth.com/specifications/specs/environmental-sensing-service-1-0/
[Environmental Sensing Profile]: https://www.bluetooth.com/specifications/specs/environmental-sensing-profile-1-0/

[BSEC License Terms and Conditions]: https://www.bosch-sensortec.com/media/boschsensortec/downloads/software/bme688_development_software/2023_04/license_terms_bme688_bme680_bsec.pdf

[`bosch,bme680`]: https://docs.zephyrproject.org/latest/build/dts/api/compatibles/bosch,bme680.html
[Fetch and Get]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/fetch_and_get.html#sensor-fetch-and-get
[Sensors]: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html
[sensor values]: https://docs.zephyrproject.org/latest/doxygen/html/structsensor__value.html

[Zephyr Module]: https://docs.zephyrproject.org/latest/develop/modules.html
[West]: https://docs.zephyrproject.org/latest/develop/west/index.html
[Workspaces]: https://docs.zephyrproject.org/latest/develop/west/workspaces.html
[Getting Started Guide]: https://docs.zephyrproject.org/latest/develop/getting_started/index.html
[manifest]: https://docs.zephyrproject.org/latest/develop/west/manifest.html
[Zephyr binary blobs]: https://docs.zephyrproject.org/latest/contribute/bin_blobs.html
[`west blobs`]: https://docs.zephyrproject.org/latest/develop/west/zephyr-cmds.html#west-blobs
[Zephyr Build System]: https://docs.zephyrproject.org/latest/build/cmake/index.html
[System Calls]: https://docs.zephyrproject.org/latest/kernel/usermode/syscalls.html
[Zephyr documentation]: https://docs.zephyrproject.org/latest/index.html
[Devicetree Bindings]: https://docs.zephyrproject.org/latest/build/dts/bindings.html
[Using an external DTS binding for an out-of-tree driver]: https://lists.zephyrproject.org/g/users/topic/using_an_external_dts_binding/78139373
[Integrate modules in Zephyr build system]: https://docs.zephyrproject.org/latest/develop/modules.html#integrate-modules-in-zephyr-build-system
[Kconfig]:https://docs.zephyrproject.org/latest/build/kconfig/index.html
[Devicetree]: https://docs.zephyrproject.org/latest/build/dts/intro.html
[devicetree overlay]: https://docs.zephyrproject.org/latest/build/dts/howtos.html#set-devicetree-overlays
[Supported Boards]: https://docs.zephyrproject.org/latest/boards/index.html
[nRF52840 DK]: https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html
[Adafruit Feather ESP32S2]: https://docs.zephyrproject.org/latest/boards/adafruit/feather_esp32s2/doc/adafruit_feather_esp32s2.html
[Settings]: https://docs.zephyrproject.org/latest/services/storage/settings/

[lib/bme68x-sensor-api]: lib/bme68x-sensor-api
[drivers/bme68x-sensor-api]: drivers/bme68x-sensor-api
[lib/bsec]: lib/bsec
[lib/bme68x-iaq]: lib/bme68x-iaq
[lib/bme68x-esp]: lib/bme68x-esp

[samples/bme68x-tphg]: samples/bme68x-tphg
[samples/bme68x-iaq]: samples/bme68x-iaq
[samples/bme68x-esp]: samples/bme68x-esp

[`bosch,bme68x-sensor-api-i2c.yaml`]: dts/bindings/bosch,bme68x-sensor-api-i2c.yaml
[`bosch,bme68x-sensor-api-spi.yaml`]: dts/bindings/bosch,bme68x-sensor-api-spi.yaml

[BME688]: https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme688/
[How to distinguish BME680 from BME688 in firmware]: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/How-to-distinguish-BME680-from-BME688-in-firmware/td-p/73929
[BME68X IAQ driver]: https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/drivers/bme68x_iaq.html
