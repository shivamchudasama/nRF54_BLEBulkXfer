target_sources(app PRIVATE
   FileSysManager.c
   FileSysManagerFSM.c
)

target_include_directories(app PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_LIST_DIR}/../Common
)

add_subdirectory(UartTestHarness)