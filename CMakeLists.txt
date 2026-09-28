#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Build configuration lives in _DI. APPLICATION_CONFIG_DIR is deliberately NOT
# set: Zephyr resolves a relative CONF_FILE against it, so the "_DI/prj.conf"
# passed by the nRF Connect VS Code extension would become "_DI/_DI/prj.conf".
# Instead, default CONF_FILE and DTC_OVERLAY_FILE here. Local variables have the
# lowest precedence in zephyr_get(), so values given on the command line or by
# the extension still win. The board is not known yet, so the overlay name uses
# an escaped ${NORMALIZED_BOARD_TARGET}, which zephyr_get() expands once
# boards.cmake has set it (e.g. nrf54l15dk_nrf54l15_cpuapp). A board without an
# overlay in _DI/boards needs one added, or DTC_OVERLAY_FILE given explicitly.
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/_DI/Kconfig)
set(CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/_DI/prj.conf)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/_DI/boards/\${NORMALIZED_BOARD_TARGET}.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nRF54_BLEBulkXfer)

add_subdirectory(_ASW)
add_subdirectory(_LIB)

