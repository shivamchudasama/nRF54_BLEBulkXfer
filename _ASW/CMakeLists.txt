file(GLOB APP_SOURCES CONFIGURE_DEPENDS
   "*.c"
)

target_sources(app PRIVATE
   ${APP_SOURCES}
)

target_include_directories(app PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}
)

add_subdirectory(_BLE)
add_subdirectory(_BLE_GENERIX)
add_subdirectory(_GENERIX)
add_subdirectory(_GAP)
add_subdirectory(_APP_LOG)
add_subdirectory(_BLK_SVC)
add_subdirectory(_DATA_STORE)
