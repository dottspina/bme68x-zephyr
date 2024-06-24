# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.5)

# Add path to the extra module if bme68x is not
# installed as an external project managed by West:
#
# list(APPEND ZEPHYR_EXTRA_MODULES
#    ${CMAKE_SOURCE_DIR}/../..
# )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sample-bme68x-tphg)

target_sources(app PRIVATE
  src/bme68x_tphg.c
  src/main.c
)

target_compile_options(app PRIVATE -Wall -Werror)
