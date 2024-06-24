# SPDX-License-Identifier: Apache-2.0

zephyr_library_named(bme68x-sensor-api)

# BME68X Sensor API headers:
zephyr_include_directories(include)
zephyr_library_include_directories(include)

zephyr_library_sources(
  src/bme68x.c
)

zephyr_library_compile_options(-Wall -Werror)
