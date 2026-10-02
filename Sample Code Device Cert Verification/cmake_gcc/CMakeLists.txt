# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	EmptyApp
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_EmptyApp target,
# which contains the content of the SLC project
include(EmptyApp.cmake)

# Add include directories for slc target
target_include_directories(slc PUBLIC
   "../_ASW"
   "../_ASW/_APP/Advertisement"
   "../_ASW/_APP/CSR"
   "../_ASW/_APP/Device_Certificate"
   "../_ASW/_APP/FSM_Lib"
   "../_ASW/_APP/GENERIX"
   "../_ASW/_APP/NVM3_Handler"
   "../_ASW/_APP/Pairing"
)

add_executable(EmptyApp)

add_subdirectory("../_ASW/_APP/Advertisement" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/Advertisement")
add_subdirectory("../_ASW/_APP/CSR" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/CSR")
add_subdirectory("../_ASW/_APP/Device_Certificate" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/Device_Certificate")
add_subdirectory("../_ASW/_APP/FSM_Lib" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/FSM_Lib")
add_subdirectory("../_ASW/_APP/GENERIX" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/GENERIX")
add_subdirectory("../_ASW/_APP/NVM3_Handler" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/NVM3_Handler")
add_subdirectory("../_ASW/_APP/Pairing" "${CMAKE_CURRENT_BINARY_DIR}/_ASW/_APP/Pairing")

target_compile_definitions(EmptyApp PUBLIC
   # Add additional macros here
)

target_compile_options(EmptyApp PUBLIC
   # Set additional compiler flags here
)

target_link_options(EmptyApp PUBLIC
   # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(EmptyApp PRIVATE
   slc
)

# Include managed project content if available
include(EmptyApp_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
   message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(EmptyApp PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET EmptyApp
   POST_BUILD
   COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.s37"
   COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.hex"
   COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.bin"
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET EmptyApp
   POST_BUILD
   WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
   COMMAND ${post_build_command}
)
endif()
