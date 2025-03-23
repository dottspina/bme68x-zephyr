# Copyright (c) 2025, Chris Duf
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library_named(bme68x-esp)

zephyr_library_include_directories(
  include
)
zephyr_include_directories(include)

zephyr_library_sources(
  src/bme68x_ess.c
  src/bme68x_esp_gap.c
  src/bme68x_esp_sensor.c
)

zephyr_library_compile_options(-Wall -Werror)
