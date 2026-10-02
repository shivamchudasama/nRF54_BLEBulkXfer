# CMakeLists.txt – Detailed Explanation

This document provides a structured, markdown-formatted explanation of the `CMakeLists.txt` file used in a Silicon Labs **EmptyApp** project managed via **Simplicity Studio 6** and the **Gecko SDK**.

---

## Overview

CMake is a build-system generator that configures how your project is compiled, linked, and packaged. This `CMakeLists.txt` file defines:

- Project metadata (name, version, languages)
- Source files and include paths
- Compiler and linker options
- Integration with Silicon Labs’ auto-generated SDK content
- Post-build artifact generation

This file **is user-editable** and is not overwritten by Simplicity Studio, unlike some auto-generated `.cmake` files.

> ⚠️ After modifying this file, you must re-run CMake (e.g., `cmake --preset project`) to regenerate build files.

---

## 1. CMake Version Requirement

```cmake
cmake_minimum_required(VERSION "3.25")
```

**Purpose**
- Ensures a minimum CMake version that supports all required features.

**Notes**
- Silicon Labs toolchains often rely on modern CMake behavior.
- Increase the version only if you need newer CMake features.

---

## 2. Project Definition

```cmake
project(
    EmptyApp
    VERSION 1.0
    LANGUAGES C CXX ASM
)
```

**Purpose**
- Declares project name, version, and enabled languages.

**Details**
- `C` – main application code
- `CXX` – optional C++ support
- `ASM` – required for Cortex-M startup and low-level code

---

## 3. Include SLC Project Definition

```cmake
include(EmptyApp.cmake)
```

**Purpose**
- Imports the auto-generated Silicon Labs Component (SLC) target.

**Important**
- `EmptyApp.cmake` defines the `slc` target containing SDK libraries.
- Do **not** edit this file manually.

---

## 4. Include Directories for `slc` Target

```cmake
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
```

**Purpose**
- Makes application headers visible to SDK sources.

**Key Point**
- `PUBLIC` propagates these include paths to targets that link against `slc`.

---

## 5. Define the Executable Target

```cmake
add_executable(EmptyApp
    "../_ASW/app.c"
    "../_ASW/_APP/Advertisement/AdvConfig.c"
    "../_ASW/_APP/Advertisement/Advertisement.c"
    # ... additional source files
)
```

**Purpose**
- Defines the final firmware executable and its source files.

**Best Practice**
- Explicitly list source files for traceability.

---

## 6. Include Directories for `EmptyApp`

```cmake
target_include_directories(EmptyApp PUBLIC
    "../_ASW"
    "../_ASW/_APP/Advertisement"
    "../_ASW/_APP/CSR"
    "../_ASW/_APP/Device_Certificate"
    "../_ASW/_APP/FSM_Lib"
    "../_ASW/_APP/GENERIX"
    "../_ASW/_APP/NVM3_Handler"
    "../_ASW/_APP/Pairing"
)
```

**Purpose**
- Ensures headers are available when compiling application sources.

---

## 7. Compile-Time Definitions

```cmake
target_compile_definitions(EmptyApp PUBLIC
    # Example: DEBUG=1
)
```

**Purpose**
- Adds preprocessor macros for conditional compilation.

---

## 8. Compiler Options

```cmake
target_compile_options(EmptyApp PUBLIC
    # Example: -O2 -Wall
)
```

**Purpose**
- Customizes compiler behavior (warnings, optimizations, etc.).

---

## 9. Linker Options

```cmake
target_link_options(EmptyApp PUBLIC
    # Example: -Wl,--gc-sections
)
```

**Purpose**
- Controls the link stage, especially important for embedded memory usage.

---

## 10. Link Against SDK Libraries

```cmake
target_link_libraries(EmptyApp PRIVATE
    slc
)
```

**Purpose**
- Links application code with Silicon Labs SDK components.

---

## 11. Include Managed Project Content (Optional)

```cmake
include(EmptyApp_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()
```

**Purpose**
- Allows Simplicity Studio to inject additional configuration automatically.

---

## 12. Force Linker Language

```cmake
set_target_properties(EmptyApp PROPERTIES LINKER_LANGUAGE C)
```

**Purpose**
- Forces the linker to use the C toolchain.

**Why It Matters**
- Avoids issues when mixing C and C++ in embedded environments.

---

## 13. Post-Build Artifact Generation

```cmake
add_custom_command(TARGET EmptyApp POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:EmptyApp>" "$<TARGET_FILE_DIR:EmptyApp>/$<TARGET_FILE_BASE_NAME:EmptyApp>.bin"
)
```

**Purpose**
- Generates firmware images used for flashing.

**Artifacts**
- `.bin` – raw binary
- `.hex` – Intel HEX
- `.s37` – Motorola S-record

---

## 14. Optional Post-Build Pipeline

```cmake
if(post_build_command)
    add_custom_command(TARGET EmptyApp POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        COMMAND ${post_build_command}
    )
endif()
```

**Purpose**
- Executes additional post-processing steps such as signing or packaging.

---

## General Guidelines

- ✅ Modify source lists, include paths, and compile options freely
- ❌ Do not edit auto-generated `.cmake` files
- 🔄 Re-run CMake after changes
- 📖 Refer to official CMake and Silicon Labs documentation for advanced usage

---

**End of document**

