cmake_minimum_required(VERSION 3.20)

# bundled vcpkg detection
if(DEFINED ENV{VCInstallDir})
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY "$ENV{VCInstallDir}/vcpkg"
            OUTPUT_VARIABLE DETECTED_VCPKG_BASELINE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(DETECTED_VCPKG_BASELINE)
            set(VCPKG_DEFAULT_BASELINE "${DETECTED_VCPKG_BASELINE}" CACHE STRING "" FORCE)
            set(ENV{VCPKG_DEFAULT_BASELINE} "${DETECTED_VCPKG_BASELINE}")
        endif()
    endif()
endif()

project(dsda_xdre_workspace C CXX)

set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)

# modular build options
option(BUILD_DSDA_DOOM "Build standalone dsda-doom executable" ON)
option(BUILD_XDRE      "Build XDRE TAS editor executable" ON)

# static crt config, should be ignored by gnu stuff
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# clang-cl overrides
if(MSVC AND (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
    add_compile_options(
        /clang:-fms-compatibility
        /clang:-fms-extensions
        /clang:-Wno-builtin-macro-redefined
        /clang:-Wno-incompatible-ms-struct
        /clang:-Wno-ignored-attributes
        /clang:-Wno-error=builtin-macro-redefined
    )
endif()

# default to release build
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type: Debug Release RelWithDebInfo" FORCE)
endif()

# add dsda dir
add_subdirectory(dsda-doom/prboom2)

# add ldre
if(BUILD_XDRE)
    add_subdirectory(xdre)
endif()