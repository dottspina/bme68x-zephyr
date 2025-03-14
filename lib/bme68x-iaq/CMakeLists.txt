# Copyright (c) 2024, Chris Duf
#
# SPDX-License-Identifier: Apache-2.0

# BSEC IAQ configurations have the form:
#   config/bme680/bme680_iaq_v_s_d
#
# BME688 configurations are AI-only.
set(bsec_conf_root
  ${CMAKE_CURRENT_SOURCE_DIR}/config/bme680
)
set(bsec_conf_vdd ${CONFIG_BME68X_IAQ_VDD})
set(bsec_conf_sr ${CONFIG_BME68X_IAQ_SAMPLE_RATE})
set(bsec_conf_calib ${CONFIG_BME68X_IAQ_CALIB_TIME})
set(bsec_conf_name
  bme680_iaq_${bsec_conf_vdd}_${bsec_conf_sr}_${bsec_conf_calib}
)
set(bsec_conf_dir ${bsec_conf_root}/${bsec_conf_name})

message(STATUS "BSEC IAQ configuration: ${bsec_conf_name}")
if(EXISTS ${bsec_conf_dir})
  message(STATUS "+ ${bsec_conf_dir}")
else()
  message(FATAL_ERROR "not found: ${bsec_conf_dir}")
endif()


zephyr_library_named(bme68x-iaq)
zephyr_library_link_libraries(bsec)

zephyr_library_include_directories(
  include
  ${bsec_conf_dir}
)
zephyr_include_directories(include)

set(bme68x_iaq_src
  ${bsec_conf_dir}/bsec_iaq.c
  src/bme68x_iaq.c
)

if(CONFIG_BME68X_IAQ_NVS)
  list(APPEND bme68x_iaq_src src/bme68x_iaq_nvs.c)
endif(CONFIG_BME68X_IAQ_NVS)

if(CONFIG_BME68X_IAQ_SETTINGS)
  list(APPEND bme68x_iaq_src src/bme68x_iaq_settings.c)
endif(CONFIG_BME68X_IAQ_SETTINGS)

zephyr_library_sources(
  ${bme68x_iaq_src}
)

zephyr_library_compile_options(-Wall -Werror)
