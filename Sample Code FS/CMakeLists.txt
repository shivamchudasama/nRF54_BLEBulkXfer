#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

message(STATUS "FileSysManager final DTC_OVERLAY_FILE: ${DTC_OVERLAY_FILE}")
message(STATUS "FileSysManager final EXTRA_CONF_FILE: ${EXTRA_CONF_FILE}")

# Sysbuild main-app path: inject app pm.yml before find_package(Zephyr)
# so Partition Manager can collect it during configuration.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/_DI/pm.yml)
  get_filename_component(APP_PM_YML ${CMAKE_CURRENT_SOURCE_DIR}/_DI/pm.yml REALPATH)
  set_property(GLOBAL APPEND PROPERTY PM_SUBSYS_PATHS ${APP_PM_YML})
  set_property(GLOBAL APPEND PROPERTY PM_SUBSYS_OUTPUT_PATHS ${CMAKE_CURRENT_BINARY_DIR}/pm.yml.app)
endif()

set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/_DI/Kconfig CACHE FILEPATH "App Kconfig root" FORCE)
find_package(Zephyr REQUIRED HINTS {ZEPHYR_BASE})
project(BLE_FOTA_PoC)

add_subdirectory(_ASW)
add_subdirectory(_LIB)

