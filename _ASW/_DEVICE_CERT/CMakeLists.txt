file(GLOB APP_SOURCES CONFIGURE_DEPENDS
   "*.c"
)

target_sources(app PRIVATE
   ${APP_SOURCES}
)

target_include_directories(app PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}
)

# mbedTLS X.509 (certificate/CSR parsing and verification). nrf_security links
# it into the zephyr library only; the app gets it through this interface target.
target_link_libraries(app PRIVATE mbedtls_external)
