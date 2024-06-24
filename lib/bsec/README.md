# `lib/bsec` ─ Bosch Sensortec Environmental Cluster (BSEC)

Bosch Sensortec Environmental Cluster ([BSEC]) processes BME680/688 sensor inputs into virtual sensor outputs for Index for Air Quality (IAQ) or gas detection (BME688 only).

This Zephyr library provides BSEC version 2.5.0.2.

See also:

- [lib/bme68x-iaq]: support library for Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API
- [samples/bme68x-iaq]: example application, IAQ with BSEC and the BME68X Sensor API

[lib/bme68x-iaq]: /lib/bme68x-iaq
[samples/bme68x-iaq]: /samples/bme68x-iaq
[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI

> [!IMPORTANT]
>
> BSEC is not available under an Open Source license: installation and use requires acceptance of these [License Terms and Conditions].

## Installation

BSEC is proprietary software protected by copyright and available only as statically linked libraries for target platforms (e.g. Cortex-M4 with GCC *linkage*).

These binary blobs (aka *libalgobsec*) cannot be included in this repository and must be downloaded:

- either from the [BME680 Product page]: supports more targets (e.g. IAR compiler), but the download process does not permit to automate installation
- or from the [BSEC2 Arduino Library] GitHub: only GCC targets, download links for tagged releases are available; this is the preferred source

> [!NOTE]
> Although `bsec_get_version()` will answer `2.5.0.2` for both sources above, the binary blobs actually differ, as do the supported targets and how they are organized.
>
> Instructions bellow directly apply to, and are tested with, the BSEC2 Arduino Library *distribution*.

[BME680 Product page]: https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/
[BSEC2 Arduino Library]: https://github.com/boschsensortec/Bosch-BSEC2-Library
[License Terms and Conditions]: https://www.bosch-sensortec.com/media/boschsensortec/downloads/software/bme688_development_software/2023_04/license_terms_bme688_bme680_bsec.pdf
[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/

By default, the build system ([CMakeLists.txt](CMakeLists.txt)) will search for BSEC binaries where it would search for [binary blobs] coming with the `bme68x` Zephyr module:

- in sub-directories of `zephyrproject/modules/lib/bme68x/zephyr/blobs` if installed as an *external project* of the West workspace at `zephyrproject`
- in sub-directories of `bme68x-zephyr/zephyr/blobs` if installed as an *extra module* given to CMake, where `bme68x-zephyr` is a clone of this project

The different targets are identified by directory names:

```
blobs
└── bsec
    ├── cortex-m3
    │   └── libalgobsec.a
    ├── cortex-m33
    │   └── libalgobsec.a
    ├── cortex-m33f
    │   └── libalgobsec.a
    ├── cortex-m4
    │   └── libalgobsec.a
    ├── cortex-m4f
    │   └── libalgobsec.a
    ├── esp32
    │   └── libalgobsec.a
    ├── esp32c3
    │   └── libalgobsec.a
    ├── esp32s2
    │   └── libalgobsec.a
    └── esp32s3
        └── libalgobsec.a
```

When the `bme68x` module is added to the external projects managed by West, BSEC static libraries can be simply installed using the [`west blobs`] command:

```
$ west blobs fetch bme68x
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m3/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m33/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m33f/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m4/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/cortex-m4f/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/esp32/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/esp32s2/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/esp32s3/libalgobsec.a
Fetching blob bme68x: /path/to/zephyrproject/modules/lib/bme68x/zephyr/blobs/bsec/esp32c3/libalgobsec.a
```

Otherwise, manually copy the needed BSEC binaries to their expected locations.

> [!NOTE]
> The `f` suffix in `cortex-m33f` and `cortex-m4f` identify targets for hardware floating-point ABI:
>
> | Target        | ABI                              |
> |---------------|----------------------------------|
> | `cortex-m33f` | `-mfpv5-sp-d16 -mfloat-abi=hard` |
> | `cortex-m4f`  | `-mfpv4-sp-d16 -mfloat-abi=hard` |

[bme68x-zephyr]: https://github.com/dottspina/bme68x-zephyr
[binary blobs]: https://docs.zephyrproject.org/latest/contribute/bin_blobs.html
[`west blobs`]: https://docs.zephyrproject.org/latest/develop/west/zephyr-cmds.html#west-blobs

## Use

This Zephyr library is enabled with the [Kconfig] configuration option `BSEC`.

[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html

### Link with BSEC

By default, the build process ([CMakeLists.txt](CMakeLists.txt)) will try to identify and locate the appropriate BSEC target based on Kconfig symbols:

| Target      | *Architecture* (Kconfig) | CPU (Kconfig)               |
|-------------|--------------------------|-----------------------------|
| cortex-m3   | `CPU_CORTEX=y`           | `CPU_CORTEX_M3=y`           |
| cortex-m33  |                          | `CPU_CORTEX_M33=y`          |
| cortex-m33f |                          | `CPU_CORTEX_M33=y`, `FPU=y` |
| cortex-m4   |                          | `CPU_CORTEX_M4=y`           |
| cortex-m4f  |                          | `CPU_CORTEX_M4=y`, `FPU=y`  |
| esp32       | `SOC_FAMILY_ESP32=y`     | `SOC_SERIES_ESP32="y"`      |
| esp32s2     |                          | `SOC_SERIES_ESP32S2="y"`    |
| esp32s3     |                          | `SOC_SERIES_ESP32S3="y"`    |
| esp32c3     |                          | `SOC_SERIES_ESP32C3="y"`    |

If the above fails or is not applicable, the build process also supports the CMake variable `LIBALGOBSEC` which allows to explicitly set the path to the target BSEC blob, e.g.:

```
$ west build /path/to/zephyr/application -- -DLIBALGOBSEC=/path/to/libalgobsec.a
```

The BSEC library is exposed as a Zephyr *interface library* named `bsec`.

To link another Zephyr library with BSEC:

``` cmake
zephyr_library()
zephyr_library_link_libraries(bsec)
```

To link an application with BSEC, the Kconfig symbol `APP_LINK_WITH_BSEC` should be defined: this is automatic when this library is enabled.

See also:

- CMake [Interface Libraries]
- the `zephyr_interface_library_named()` function in [zephyr/cmake/modules/extensions.cmake]

[Interface Libraries]: https://cmake.org/cmake/help/v3.20/command/add_library.html#interface-libraries
[zephyr/cmake/modules/extensions.cmake]: https://github.com/zephyrproject-rtos/zephyr/blob/main/cmake/modules/extensions.cmake

### API

Enabling this Zephyr library makes the BSEC API header files directly accessible by application code.

| Header                       | API                                                |
|------------------------------|----------------------------------------------------|
| [`bsec_datatypes.h`]       | Data types and defines used by interface functions |
| [`bsec_interface.h`]       | Declaration of standard interface functions        |
| [`bsec_interface_multi.h`] | Declaration of multi-interface functions           |

[`bsec_datatypes.h`]: include/bsec_datatypes.h
[`bsec_interface.h`]: include/bsec_interface.h
[`bsec_interface_multi.h`]: include/bsec_interface_multi.h

> [!IMPORTANT]
>
> The BSEC library works with state data in the `bss` section, which by default is not accessible to [User Mode] threads, causing MPU faults:
>
> ```
> [00:00:00.257,476] <inf> bme68x_sensor_api: bme680@77 (Fixed-point API)
> [00:00:00.274,505] <err> os: ***** MPU FAULT *****
> [00:00:00.274,505] <err> os:   Data Access Violation
> [00:00:00.274,505] <err> os:   MMFAR Address: 0x20000f20
> ```
>
> ```
> (gdb) i symbol 0x20000f20
> bsec_library in section bss
> ```

[User Mode]: https://docs.zephyrproject.org/latest/kernel/usermode/index.html
[Memory Domains]: https://docs.zephyrproject.org/latest/kernel/usermode/memory_domain.html#memory-domains
