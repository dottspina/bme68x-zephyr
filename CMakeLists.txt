# SPDX-License-Identifier: Apache-2.0

# See https://docs.zephyrproject.org/latest/develop/modules.html#zephyr-modules.
#
list(APPEND SYSCALL_INCLUDE_DIRS
  # System calls for I2C/SPI transfers (internal).
  ${ZEPHYR_BME68X_MODULE_DIR}/drivers/bme68x-sensor-api/src
  # System calls for Non Volatile Storage (BSEC state persistence).
  ${ZEPHYR_BME68X_MODULE_DIR}/lib/bme68x-iaq/include
)
set(SYSCALL_INCLUDE_DIRS ${SYSCALL_INCLUDE_DIRS} PARENT_SCOPE)

add_subdirectory(lib)
add_subdirectory(drivers)
