cmake_minimum_required(VERSION 3.16)
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

target_compile_definitions(xdre PRIVATE
    XDRE=1
    PACKAGE_STRING="LDRE alpha"
    HAVE_CONFIG_H
)

target_link_libraries(xdre PRIVATE
    dsda-doom-core
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