file(GLOB APP_SOURCES CONFIGURE_DEPENDS
   "*.c"
)

target_sources(app PRIVATE
   ${APP_SOURCES}
)

target_include_directories(app PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}
)

add_subdirectory(_FOTA_TP)
add_subdirectory(_FOTA_FSM)
add_subdirectory(_FOTA_GATT_DB)
add_subdirectory(_FOTA_Event)