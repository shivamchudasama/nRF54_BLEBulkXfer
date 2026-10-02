file(GLOB APP_SOURCES CONFIGURE_DEPENDS
   "*.c"
)

target_sources(EmptyApp PRIVATE
   ${APP_SOURCES}
)

target_include_directories(EmptyApp PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}
)
