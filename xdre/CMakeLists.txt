cmake_minimum_required(VERSION 3.20)
project(xdre C CXX)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsda-doom/prboom2/src
    ${CMAKE_CURRENT_BINARY_DIR}/../dsda-doom/build-config
    ${SDL2_INCLUDE_DIRS}
    ${wxWidgets_INCLUDE_DIRS}
)

set(XDRE_SOURCES
    src/mainStuff.cpp
    src/MainWindow.cpp
    src/TiclistCtrl.cpp
    src/BruteDialog.cpp
    src/HeaderDialog.cpp
    src/KeysDialog.cpp
    src/xdre.cpp
    src/cstuff.c
    src/dsda_compat.c
)

add_executable(xdre
    ${XDRE_SOURCES}
)

# defs
target_compile_definitions(xdre PRIVATE
    XDRE=1
    PACKAGE_STRING="LDRE beta v0.2.0"
    HAVE_CONFIG_H
    _FORTIFY_SOURCE=3
    $<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_ASSERTIONS>
)

# custom compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(xdre PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:
            -O2
            -g
            -fstack-protector-strong
            -fstack-clash-protection
            -fno-strict-aliasing
            -fwrapv
            -Wall
            -Wextra
            -Wnull-dereference
            -Warray-bounds=2
            -Wstringop-overflow=4
            -Wno-unused-parameter
        >
    )
endif()

# link the xdre version of dsda core
target_link_libraries(xdre PRIVATE
    dsda-doom-core-xdre
    ${wxWidgets_LIBRARIES}
    ${SDL2_LIBRARIES}
)

if(WIN32)
    target_link_libraries(xdre PRIVATE ws2_32 winmm)
    if(MSVC)
        set_target_properties(xdre PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    else()
        set_target_properties(xdre PROPERTIES LINK_FLAGS "-mconsole")
    endif()
endif()