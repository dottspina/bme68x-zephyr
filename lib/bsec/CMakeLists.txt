# Copyright (c) 2024, Chris Duf
#
# SPDX-License-Identifier: Apache-2.0

# Allows users to explicitly set the path to the BSEC2 static library
# blob to link with.
set(LIBALGOBSEC "" CACHE FILEPATH "Path to BSEC library blob (libalgobsec.a)")

# West-managed binary blobs directory.
#
# See:
# - https://docs.zephyrproject.org/latest/contribute/bin_blobs.html
# - zephyr/module.yml

set(bsec_blobs_dir ${ZEPHYR_BME68X_MODULE_DIR}/zephyr/blobs/bsec)
message(STATUS "BSEC blobs root: ${bsec_blobs_dir}")

# Arm Cortex-M.
#
# Set BSEC target for CPU_CORTEX_XX builds.
#
# When hardware floatint-point unit (FPU) is enabled,
# the binary with hardware floating-point ABI is selected.
function(set_bsec_target_cortex)
  # Cortex-M3
  if(CONFIG_CPU_CORTEX_M3)
    set(bsec_target "cortex-m3" PARENT_SCOPE)
  # Cortex-M33
  elseif(CONFIG_CPU_CORTEX_M33)
    if(CONFIG_FPU)
      set(bsec_target "cortex-m33f" PARENT_SCOPE)
    else()
      set(bsec_target "cortex-m33" PARENT_SCOPE)
    endif(CONFIG_FPU)
  # Cortex-M4
  elseif(CONFIG_CPU_CORTEX_M4)
    if(CONFIG_FPU)
      set(bsec_target "cortex-m4f" PARENT_SCOPE)
    else()
      set(bsec_target "cortex-m4" PARENT_SCOPE)
    endif(CONFIG_FPU)
  endif()
endfunction()

# All ESP32.
#
# Set BSEC target for SOC_FAMILY_ESP32:
# - xtensa: ESP32, ESP32S2, ESP32S3
# - riscv: ESP32C3
function(set_bsec_target_esp32)
  if (CONFIG_SOC_SERIES_ESP32S2)
    set(bsec_target "esp32s2" PARENT_SCOPE)
  elseif(CONFIG_SOC_SERIES_ESP32S3)
    set(bsec_target "esp32s3" PARENT_SCOPE)
  elseif(CONFIG_SOC_SERIES_ESP32C3)
    set(bsec_target "esp32c3" PARENT_SCOPE)
  else()
    set(bsec_target "esp32" PARENT_SCOPE)
  endif()
endfunction()


# Try to find the appropriate BSEC library blob to link with.
function(find_libalgobsec)
  set(bsec_target "")
  if (CONFIG_CPU_CORTEX)
    set_bsec_target_cortex()
  elseif(CONFIG_SOC_FAMILY_ESP32)
    set_bsec_target_esp32()
  endif()

  if (bsec_target)
    message(STATUS "BSEC target: ${bsec_target}")
    set(LIBALGOBSEC "${bsec_blobs_dir}/${bsec_target}/libalgobsec.a" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Failed to guess BSEC target, try -DLIBALGOBSEC=/path/to/libalgobsec.a")
  endif()
endfunction()

if (NOT LIBALGOBSEC)
  find_libalgobsec()
endif()
if (EXISTS ${LIBALGOBSEC})
  message(STATUS "BSEC blob: ${LIBALGOBSEC}")
else()
  message(FATAL_ERROR "BSEC blob not found: ${LIBALGOBSEC}")
endif()

# BSEC binary blob (libalgobsec) and API headers.
add_library(algobsec STATIC IMPORTED GLOBAL)
set_target_properties(algobsec
  PROPERTIES IMPORTED_LOCATION
  ${LIBALGOBSEC}
)
target_include_directories(algobsec
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Zephyr interface library.
zephyr_interface_library_named(bsec)
target_link_libraries(bsec
  INTERFACE
  algobsec
)
