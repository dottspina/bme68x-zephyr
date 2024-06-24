# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_BME68X_SENSOR_API bme68x-sensor-api)
add_subdirectory_ifdef(CONFIG_BSEC bsec)
add_subdirectory_ifdef(CONFIG_BME68X_IAQ bme68x-iaq)
