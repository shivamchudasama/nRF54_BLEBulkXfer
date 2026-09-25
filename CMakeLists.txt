#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)
set(APPLICATION_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/_DI)
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/_DI/Kconfig)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nRF54_BLEBulkXfer)

add_subdirectory(_ASW)
add_subdirectory(_LIB)

