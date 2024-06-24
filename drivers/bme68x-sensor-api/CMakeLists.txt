# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# <drivers/bme68x_sensor_api.h>
zephyr_library_include_directories(include)
zephyr_include_directories(include)

zephyr_library_sources(
    src/bme68x_drv.c
    src/bme68x_drv_i2c.c 
    src/bme68x_drv_spi.c
)

zephyr_library_compile_options(-Wall -Werror)
