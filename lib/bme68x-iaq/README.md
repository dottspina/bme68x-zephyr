# `lib/bme68x-iaq` ─ Support library for Index for Air Quality (IAQ) with BSEC

Support library for Index for Air Quality with Bosch Sensortec Environmental Cluster ([BSEC]) and the [BME68X Sensor API].

[BSEC]: https://www.bosch-sensortec.com/software-tools/software/bme680-software-bsec/
[BME68X Sensor API]: https://github.com/boschsensortec/BME68x_SensorAPI

Provides the *boiler plate* code and configuration artifacts necessary to:

- initialize and configure the BSEC algorithm for IAQ
- run the BSEC control loop with a single BME680/688 sensor,
  receiving IAQ estimates through a callback
- manage BSEC state persistence to flash storage

See also:

- [samples/bme68x-iaq]: example application, Index for Air Quality (IAQ) with BSEC and the BME68X Sensor API

[samples/bme68x-iaq]: /samples/bme68x-iaq

## Configuration

This library should be enabled with [Kconfig].

| [`Kconfig`]                    | Option                               |
|--------------------------------|--------------------------------------|
| `BME68X_IAQ (=n)`              | Enable Support library for BSEC IAQ  |
| `BME68X_IAQ_NVS (=n)`          | Enable BSEC state persistence to NVS |

Configuration options are accessible via the Kconfig menu: `Modules → bme68x → [*] Support library for BSEC IAQ`.

[`Kconfig`]: Kconfig
[Kconfig]: https://docs.zephyrproject.org/latest/build/kconfig/index.html

> [!IMPORTANT]
>
> The size of the stack must be adjusted to accommodate the BSEC working buffers (more than 4 kB), e.g. `CONFIG_MAIN_STACK_SIZE=8192`.

### BSEC algorithm

Refer to [lib/bsec] for BSEC algorithm installation.

Predefined BSEC configurations for IAQ licensed under the BSD-3-Clause license are included.

The BSEC algorithm configuration is selected with Kconfig options:

| Option           | Value          | Kconfig                               |
|------------------|----------------|---------------------------------------|
| Supply voltage   | 3.3 V          | `BME68X_IAQ_33V` (default)            |
|                  | 1.8 V          | `BME68X_IAQ_18V`                      |
| Calibration time | 4 days         | `BME68X_IAQ_4D` (default)             |
|                  | 28 days        | `BME68X_IAQ_28D`                      |
| BSEC sample rate | 1/3 Hz (LP)    | `BME68X_IAQ_SAMPLE_RATE_LP` (default) |
|                  | 1/300 Hz (ULP) | `BME68X_IAQ_SAMPLE_RATE_ULP`          |

Together, they determine the directory that contains the C header and source files for the selected BSEC configuration, e.g. `bme680_iaq_33v_3s_4d` for 3.3 V, LP, and 4 days:

```
config
└── bme680
    ├── bme680_iaq_18v_300s_28d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_18v_300s_4d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_18v_3s_28d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_18v_3s_4d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_33v_300s_28d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_33v_300s_4d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    ├── bme680_iaq_33v_3s_28d
    │   ├── bsec_iaq.c
    │   └── bsec_iaq.h
    └── bme680_iaq_33v_3s_4d
        ├── bsec_iaq.c
        └── bsec_iaq.h
```

These options are accessible via the Kconfig menu: `Modules → bme68x → [*] Support library for BSEC IAQ → IAQ configuration`.

[lib/bsec]: /zephyr/lib/bsec

### Non-Volatile Storage

This library relies on Zephyr [Non-Volatile Storage (NVS)] for BSEC state persistence.

The NVS file-system expects a partition with [DT node label] `bsec_partition` that can accommodate two flash pages (two *sectors* of one page each).

[DT node label]: https://docs.zephyrproject.org/latest/build/dts/intro-syntax-structure.html#dt-node-labels

A simple approach is to resize the *storage* partition which should exist for all boards that support NVS.

For example, the [nRF52840 DK] has 1MB of flash storage partitioned as bellow in the board's DTS:

``` dts
    &flash0 {
        partitions {
            compatible = "fixed-partitions";
            #address-cells = <1>;
            #size-cells = <1>;

            boot_partition: partition@0 {
                label = "mcuboot";
                reg = <0x00000000 0x0000C000>;
            };
            slot0_partition: partition@c000 {
                label = "image-0";
                reg = <0x0000C000 0x00076000>;
            };
            slot1_partition: partition@82000 {
                label = "image-1";
                reg = <0x00082000 0x00076000>;
            };
            storage_partition: partition@f8000 {
                label = "storage";
                reg = <0x000f8000 0x00008000>;
            };
        };
    };
```

| Partition | Starts       | Ends         | Size                   |
|-----------|--------------|--------------|------------------------|
| *mcuboot* | `0x00000000` | `0x0000bfff` | `0x0000c000` (48 kB)   |
| *image-0* | `0x0000c000` | `0x00081fff` | `0x00076000` (472 kB)  |
| *image-1* | `0x00082000` | `0x000f7fff` | `0x00076000` (472 kB)  |
| *storage* | `0x000f8000` | `0x000fffff` | `0x00008000` (*32 kB*) |

Assuming a page size of 4 kB, we could rearrange the partitions like bellow:

| Partition        | Starts       | Ends         | Size                            |
|------------------|--------------|--------------|---------------------------------|
| *storage*        | `0x000f8000` | `0x000fdfff` | `0x00006000` (resized, *24 kB*) |
| `bsec_partition` | `0x000fe000` | `0x000fffff` | `0x00002000` (new, **8 kB**)    |

Example of DTS overlay fragment:

``` dts
/delete-node/ &storage_partition;
 &flash0 {
    partitions {
        /* Resize "storage" partition. */
        storage_partition: partition@f8000 {
            label = "storage";
            reg = < 0xf8000 0x6000 >;
        };
        /* Partition for BSEC state persistence (NVS). */
        bsec_partition: partition@fe000 {
            reg = < 0xfe000 0x2000 >;
        };
    };
};

```
> [!WARNING]
>
> Initializing an NVS file system on the given partition may run some *Garbage Collector*, erasing or corrupting existing data.

Refer to [Flash wear] to compute the expected Flash storage lifetime: for example, with a typical BSEC IAQ state size of 221 bytes, the NVS service will erase a flash page roughly every 35 state saves.

The [boards](boards) directory contains example DTS overlay and configuration files for [nRF52840 DK].

[`NVS`]: https://docs.zephyrproject.org/latest/kconfig.html#CONFIG_NVS
[Non-Volatile Storage (NVS)]: https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html
[Fixed flash partitions]: https://docs.zephyrproject.org/latest/build/dts/api/api.html#fixed-flash-partitions
[Flash wear]: https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html#flash-wear
[nRF52840 DK]: https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html
[`nrf52840dk_nrf52840.overlay`]: boards/nrf52840dk_nrf52840.overlay

## API

| API                  | Description                   |
|----------------------|-------------------------------|
| [`bme68x_iaq.h`]     | Support API for BSEC IAQ mode |
| [`bme68x_iaq_nvs.h`] | BSEC state persistence to NVS |

[`bme68x_iaq.h`]: include/bme68x_iaq.h
[`bme68x_iaq_nvs.h`]: include/bme68x_iaq_nvs.h

See [samples/bme68x-iaq] for a complete example application.

### IAQ mode

BSEC IAQ mode with [`bme68x_iaq.h`]:

- temperature, pressure, humidity and gas resistance from a single BME680/688 device as physical sensors (aka BSEC inputs)
- all virtual sensors (aka BSEC outputs) supported in IAQ mode
- BSEC state persistence: loads state on initialization if available, saves state with configurable periodicity (`BME68X_IAQ_STATE_SAVE_INTVL`) once the IAQ control loop is started

1. Implement the IAQ output handler:

``` C
/* IAQ output handler. */
void iaq_output_handler(struct bme68x_iaq_sample const *iaq_sample)
{
    /*
     * Do something with the IAQ sample.
     */
    if ((iaq_sample->iaq > 400) && (iaq_sample->iaq_accuracy >= BME68X_IAQ_ACCURACY_MEDIUM))  {
        printf("Run away!\n");
    }
}
```

2. Bind BME68X Sensor API sensor to device driver instance:

``` C
    /* Any compatible device will be fine. */
    struct device const *const dev = DEVICE_DT_GET_ONE(bosch_bme68x_sensor_api);

    /* Bind BME68X Sensor API sensor to Zephyr device driver instance. */
    struct bme68x_dev bme68x_dev = {0};
    bme68x_sensor_api_init(dev, &bme68x_dev);

    /* Initialize sensor as usual with BME68X Sensor API. */
    bme68x_init(&bme68x_dev);
```

3. Initialize and configure BSEC algorithm, possibly restoring saved state from flash storage:

``` C
    bme68x_iaq_init();
```

3. Run the BSEC control loop until unrecoverable error, receiving IAQ output samples when available, periodically saving state according to `BME68X_IAQ_STATE_SAVE_INTVL`:

``` C
    bme68x_iaq_run(&bme68x_dev, iaq_output_handler);
```

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
[System Calls]: https://docs.zephyrproject.org/latest/kernel/usermode/syscalls.html

### BSEC state persistence

The persistence API [`bme68x_iaq_nvs.h`] can also be used independently:

- the application must first call `bme68x_iaq_nvs_init()` to initialize NVS support
- then `bme68x_iaq_nvs_read_state()` and `bsec_set_state()` to load saved state
- `bsec_get_state()` and `bme68x_iaq_nvs_write_state()` to save current state
