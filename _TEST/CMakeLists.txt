# SPDX-License-Identifier: MIT
#
# Host test project: builds the libraries in _LIB and the testable application
# modules in _ASW for the PC, against the Zephyr shim in shim/, with Unity.
#
#   cmake -S _TEST -B build_test [-DSANITIZE=ON] [-DCOVERAGE=ON]
#   cmake --build build_test
#   ctest --test-dir build_test --output-on-failure
#
# Every test writes JUnit XML to build_test/report/junit/<test>.xml.

cmake_minimum_required(VERSION 3.20)
project(nRF54_BLEBulkXfer_HostTests C)
enable_testing()

option(SANITIZE "Build with AddressSanitizer and UBSan (gcc/clang, Linux/macOS)" OFF)
option(COVERAGE "Build with gcov instrumentation (for gcovr)" OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SHIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shim)
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPORT_DIR ${CMAKE_CURRENT_BINARY_DIR}/report)
file(MAKE_DIRECTORY ${REPORT_DIR}/junit)

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

# ---- Unity (only src/unity.c is used; its own CMakeLists is skipped) ---------
include(FetchContent)
FetchContent_Declare(unity
   URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.1.tar.gz
   URL_HASH SHA256=b41a66d45a6b99758fb3202ace6178177014d52fc524bf1f72687d93e9867292
   SOURCE_SUBDIR _no_cmake_
   DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(unity)
add_library(unity STATIC ${unity_SOURCE_DIR}/src/unity.c)
target_include_directories(unity PUBLIC ${unity_SOURCE_DIR}/src)

# ---- Compiler options for everything below -----------------------------------
set(TEST_C_FLAGS -std=gnu99 -Wall -Wextra -Werror -Wno-missing-field-initializers)
if(SANITIZE)
   add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
      -fno-omit-frame-pointer)
   add_link_options(-fsanitize=address,undefined)
endif()
if(COVERAGE)
   add_compile_options(--coverage -O0)
   add_link_options(--coverage)
endif()

# ---- Golden wire vectors (vectors/wire.json -> gen/wire_vectors.h) -----------
add_custom_command(
   OUTPUT ${GEN_DIR}/wire_vectors.h
   COMMAND Python3::Interpreter ${TOOLS_DIR}/gen_vectors.py
      ${CMAKE_CURRENT_SOURCE_DIR}/vectors/wire.json ${REPO_ROOT} ${GEN_DIR}/wire_vectors.h
   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vectors/wire.json ${TOOLS_DIR}/gen_vectors.py
      ${REPO_ROOT}/AA00000100.hex
   COMMENT "Generating wire_vectors.h")
add_custom_target(wire_vectors DEPENDS ${GEN_DIR}/wire_vectors.h)

# ---- Shim runtime (log capture, assert trap, thread runner) ------------------
add_library(zephyr_sim STATIC ${SHIM_DIR}/zephyr_sim.c)
target_include_directories(zephyr_sim PUBLIC ${SHIM_DIR})
target_compile_options(zephyr_sim PRIVATE ${TEST_C_FLAGS})

# add_unit_test(<name> SOURCES ... [INCLUDES ...] [DEFINES ...] [ARGS ...])
#   One Unity executable, registered with CTest through tools/run_unity.py.
function(add_unit_test NAME)
   cmake_parse_arguments(T "" "" "SOURCES;INCLUDES;DEFINES;ARGS" ${ARGN})
   add_executable(${NAME} ${T_SOURCES})
   # shim/ comes first so its zephyr/*.h and AppLog.h win
   target_include_directories(${NAME} PRIVATE ${SHIM_DIR} ${GEN_DIR} ${T_INCLUDES})
   target_compile_definitions(${NAME} PRIVATE ${T_DEFINES})
   target_compile_options(${NAME} PRIVATE ${TEST_C_FLAGS})
   target_link_libraries(${NAME} PRIVATE unity zephyr_sim)
   add_dependencies(${NAME} wire_vectors)
   add_test(NAME ${NAME}
      COMMAND Python3::Interpreter ${TOOLS_DIR}/run_unity.py
         ${REPORT_DIR}/junit/${NAME}.xml ${NAME} $<TARGET_FILE:${NAME}> ${T_ARGS})
endfunction()

set(BLK_DIR ${REPO_ROOT}/_LIB/BulkXfer)
set(GATT_CB_DIR ${REPO_ROOT}/_LIB/GATT_CB)
set(ASW_DIR ${REPO_ROOT}/_ASW)
set(BLK_INCLUDES ${BLK_DIR} ${ASW_DIR}/_BLE_GENERIX ${CMAKE_CURRENT_SOURCE_DIR}/unit/BulkXfer)

# ---- _LIB/BulkXfer -----------------------------------------------------------
add_unit_test(bulkxfer_frame
   SOURCES unit/BulkXfer/test_frame.c ${BLK_DIR}/BulkXfer_Frame.c
   INCLUDES ${BLK_INCLUDES})

# The engine test #includes Core/Server/Client .c; built once per role set
foreach(ROLE both server client)
   set(ROLE_DEFS "")
   if(ROLE STREQUAL "server")
      set(ROLE_DEFS BLK_ENABLE_CLIENT=0)
   elseif(ROLE STREQUAL "client")
      set(ROLE_DEFS BLK_ENABLE_SERVER=0)
   endif()
   add_unit_test(bulkxfer_engine_${ROLE}
      SOURCES unit/BulkXfer/test_engine.c ${BLK_DIR}/BulkXfer_Frame.c
      INCLUDES ${BLK_INCLUDES}
      DEFINES CONFIG_BT_GATT_CLIENT BLK_RX_POOL_DEPTH=6 ${ROLE_DEFS})
endforeach()

# ---- _LIB/GATT_CB ------------------------------------------------------------
add_unit_test(gatt_cb
   SOURCES unit/GATT_CB/test_gatt_cb.c ${GATT_CB_DIR}/GATT_GenericCallbacks.c
   INCLUDES ${GATT_CB_DIR})

# ---- _ASW/_DATA_STORE --------------------------------------------------------
# test_datastore.c #includes DataStore.c with the BulkXfer Server stubbed out;
# built with the summary line (default) and with the full hex dump.
set(DS_INCLUDES ${ASW_DIR}/_DATA_STORE ${ASW_DIR}/_BLK_SVC ${BLK_INCLUDES})
add_unit_test(datastore
   SOURCES unit/DataStore/test_datastore.c
   INCLUDES ${DS_INCLUDES})
add_unit_test(datastore_hexdump
   SOURCES unit/DataStore/test_datastore.c
   INCLUDES ${DS_INCLUDES}
   DEFINES CONFIG_DS_HEX_DUMP=1)

# Hex upload end to end: real BulkXfer Server + DataStore, simulated link and
# peer client, fed with the first segment of AA00000100.hex
add_unit_test(hex_upload_e2e
   SOURCES unit/DataStore/test_upload_e2e.c ${BLK_DIR}/BulkXfer_Frame.c
   INCLUDES ${DS_INCLUDES}
   DEFINES CONFIG_BT_GATT_CLIENT BLK_ENABLE_CLIENT=0)

# ---- _ASW/_GENERIX -----------------------------------------------------------
add_unit_test(helpers
   SOURCES unit/Helpers/test_helpers.c
   INCLUDES ${ASW_DIR}/_GENERIX)

# ---- Header hygiene: each header of a tested unit compiles on its own -------
set(CHECKED_HEADERS
   ${BLK_DIR}/BulkXfer.h ${BLK_DIR}/BulkXfer_Client.h ${BLK_DIR}/BulkXfer_Config.h
   ${BLK_DIR}/BulkXfer_Frame.h ${BLK_DIR}/BulkXfer_Server.h ${BLK_DIR}/BulkXfer_Types.h
   ${BLK_DIR}/BulkXfer_Uuid.h
   ${GATT_CB_DIR}/GATT_CB_Types.h ${GATT_CB_DIR}/GATT_GenericCallbacks.h
   ${ASW_DIR}/_BLK_SVC/BulkSvc.h ${ASW_DIR}/_DATA_STORE/DataStore.h
   ${ASW_DIR}/_GENERIX/Helpers.h)
set(HDR_INCLUDES "")
foreach(DIR ${SHIM_DIR} ${BLK_INCLUDES} ${GATT_CB_DIR} ${DS_INCLUDES} ${ASW_DIR}/_GENERIX)
   list(APPEND HDR_INCLUDES -I${DIR})
endforeach()
add_test(NAME header_hygiene
   COMMAND Python3::Interpreter ${TOOLS_DIR}/check_headers.py
      ${REPORT_DIR}/junit/header_hygiene.xml ${CMAKE_CURRENT_SOURCE_DIR}/known_header_issues.txt
      ${CMAKE_C_COMPILER} ${TEST_C_FLAGS} ${HDR_INCLUDES} -- ${CHECKED_HEADERS})
