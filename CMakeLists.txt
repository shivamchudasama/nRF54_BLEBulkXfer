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

# External flash interface of the FAT volume: FLASH_IF=spi (default) or sqspi
# (the FLPR soft peripheral), e.g. west build ... -- -DFLASH_IF=sqspi. It adds
# _DI/conf/flash_<if>.conf and _DI/boards/<board>_flash_<if>.overlay as extra
# files; Zephyr merges extra files from every scope, so those given on the
# command line or by the extension still apply. Under sysbuild a -D option
# reaches this image only through the sysbuild cache, which Zephyr reads in
# find_package(), so it is read here directly.
if(NOT DEFINED FLASH_IF AND DEFINED SYSBUILD_CACHE AND EXISTS "${SYSBUILD_CACHE}")
   file(STRINGS "${SYSBUILD_CACHE}" FLASH_IF_ENTRY REGEX "^FLASH_IF:[^=]*=")
   string(REGEX REPLACE "^FLASH_IF:[^=]*=" "" FLASH_IF "${FLASH_IF_ENTRY}")
endif()
if(NOT FLASH_IF)
   set(FLASH_IF spi)
endif()
if(NOT FLASH_IF MATCHES "^(spi|sqspi)$")
   message(FATAL_ERROR "FLASH_IF must be spi or sqspi, not '${FLASH_IF}'")
endif()
message(STATUS "External flash interface: ${FLASH_IF}")
# The flash files are added from FLASH_IF alone. Given again by the build
# configuration (as the VS Code extension does when one is picked in its
# "Base devicetree overlays"/"Extra Kconfig fragments" lists), the same file
# appears twice under different spellings and ninja fails with "... is defined
# as an output multiple times"; given as DTC_OVERLAY_FILE it also replaces the
# board overlay. Stop early with the fix instead.
set(USER_BUILD_FILES)
foreach(var DTC_OVERLAY_FILE EXTRA_DTC_OVERLAY_FILE CONF_FILE EXTRA_CONF_FILE)
   list(APPEND USER_BUILD_FILES "$CACHE{${var}}")
   if(DEFINED SYSBUILD_CACHE AND EXISTS "${SYSBUILD_CACHE}")
      file(STRINGS "${SYSBUILD_CACHE}" USER_ENTRY REGEX "^${var}:[^=]*=")
      list(APPEND USER_BUILD_FILES "${USER_ENTRY}")
   endif()
endforeach()
if("${USER_BUILD_FILES}" MATCHES "flash_(spi|sqspi)\\.(overlay|conf)")
   message(FATAL_ERROR
      "Do not pass _DI/conf/flash_<if>.conf or _DI/boards/<board>_flash_<if>.overlay "
      "to the build: they are added from FLASH_IF. Remove them from the build "
      "configuration (base overlay: _DI/boards/<board>.overlay or none; no extra "
      "files) and select the interface with -DFLASH_IF=spi|sqspi.")
endif()
set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/_DI/conf/flash_${FLASH_IF}.conf)
set(EXTRA_DTC_OVERLAY_FILE
   ${CMAKE_CURRENT_SOURCE_DIR}/_DI/boards/\${NORMALIZED_BOARD_TARGET}_flash_${FLASH_IF}.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ProjectHanuman)

add_subdirectory(_ASW)
add_subdirectory(_LIB)

# Per-module memory report: after the final link, _TOOLS/MemReport splits the
# FLASH/RAM usage in the map file by module of _ASW and _LIB, prints it below
# the linker's summary and writes it to zephyr/mem_report.txt. It runs again
# only when the image is relinked.
set(MEM_REPORT_PY ${CMAKE_CURRENT_SOURCE_DIR}/_TOOLS/MemReport/mem_report.py)
set(MEM_REPORT_TXT ${ZEPHYR_BINARY_DIR}/mem_report.txt)
add_custom_command(
   OUTPUT ${MEM_REPORT_TXT}
   COMMAND ${PYTHON_EXECUTABLE} ${MEM_REPORT_PY}
      --map ${ZEPHYR_BINARY_DIR}/${KERNEL_MAP_NAME}
      --root ${CMAKE_CURRENT_SOURCE_DIR} --dirs _ASW _LIB
      --out ${MEM_REPORT_TXT}
   DEPENDS ${logical_target_for_zephyr_elf} ${MEM_REPORT_PY}
   COMMENT "Splitting memory usage by module"
   VERBATIM
)
add_custom_target(mem_report ALL DEPENDS ${MEM_REPORT_TXT})

