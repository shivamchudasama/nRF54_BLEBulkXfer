if(CONFIG_UART_TEST_HARNESS)
   target_sources(app PRIVATE
      UartTestHarness.c
   )
endif()

target_include_directories(app PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_LIST_DIR}/..
   ${CMAKE_CURRENT_LIST_DIR}/../../Common
)

