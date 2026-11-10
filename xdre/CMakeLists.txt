cmake_minimum_required(VERSION 3.20)
project(xdre C CXX)

# opengl and sdl2
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

set(XDRE_SOURCES
    # LDRE
    src/mainStuff.cpp
    src/MainWindow.cpp
    src/xdre.cpp
    src/ldre_c.c
    src/dsda_compat.c

    # ImGui core
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp

    # ImGui backends
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(xdre
    ${XDRE_SOURCES}
)

# resolve sdl2 static target without causing build issue on gnu
if(MSVC)
    if(TARGET SDL2::SDL2-static)
        set(XDRE_SDL2_TARGET SDL2::SDL2-static)
    elseif(TARGET SDL2::SDL2)
        set(XDRE_SDL2_TARGET SDL2::SDL2)
    else()
        set(XDRE_SDL2_TARGET ${SDL2_LIBRARIES})
    endif()
else()
    if(TARGET SDL2::SDL2)
        set(XDRE_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(XDRE_SDL2_TARGET SDL2::SDL2-static)
    else()
        set(XDRE_SDL2_TARGET ${SDL2_LIBRARIES})
    endif()
endif()

# collect sdl2 includes
set(XDRE_SDL_INCS "")
if(TARGET ${XDRE_SDL2_TARGET})
    get_target_property(_target_sdl_inc ${XDRE_SDL2_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    if(_target_sdl_inc)
        list(APPEND XDRE_SDL_INCS ${_target_sdl_inc})
    endif()
endif()
if(SDL2_INCLUDE_DIRS)
    list(APPEND XDRE_SDL_INCS ${SDL2_INCLUDE_DIRS})
endif()

set(XDRE_EXPANDED_SDL_INCS "")
foreach(_inc ${XDRE_SDL_INCS})
    list(APPEND XDRE_EXPANDED_SDL_INCS "${_inc}")
    if(EXISTS "${_inc}/SDL2")
        list(APPEND XDRE_EXPANDED_SDL_INCS "${_inc}/SDL2")
    endif()
endforeach()
list(REMOVE_DUPLICATES XDRE_EXPANDED_SDL_INCS)

target_include_directories(xdre PRIVATE
    src
    src/imgui
    src/imgui/backends
    ${XDRE_EXPANDED_SDL_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsda-doom/prboom2/src
    ${CMAKE_CURRENT_BINARY_DIR}/../dsda-doom/build-config
)

target_compile_definitions(xdre PRIVATE
    XDRE=1
    PACKAGE_STRING="LDRE beta v0.4.0"
    HAVE_CONFIG_H
)

# distinctive compiler flags for platforms
if(MSVC)
    target_compile_definitions(xdre PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(xdre PRIVATE
        /W3
        /wd4996
        /wd4244
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(xdre PRIVATE
            /clang:-Wno-unused-parameter
            /clang:-Wno-null-dereference
            /clang:-Wno-array-bounds
        )
    endif()
else()
    target_compile_definitions(xdre PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_ASSERTIONS>
        $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    )
    target_compile_options(xdre PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:
            -O2
            -g
            -fstack-protector-strong
            -fno-strict-aliasing
            -fwrapv
            -Wall
            -Wextra
            -Wnull-dereference
            -Wno-unused-parameter
        >
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(xdre PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:
                -fstack-clash-protection
                -Warray-bounds=2
                -Wstringop-overflow=4
            >
        )
    endif()
endif()

target_link_libraries(xdre PRIVATE
    dsda-doom-core-xdre
    OpenGL::GL
    ${XDRE_SDL2_TARGET}
)

if(WIN32)
    target_link_libraries(xdre PRIVATE ws2_32 winmm imm32 comdlg32 version setupapi)
    if(MSVC)
        set_target_properties(xdre PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    else()
        set_target_properties(xdre PROPERTIES LINK_FLAGS "-mconsole")
    endif()
endif()

# auto copy wad
if(TARGET dsda-doom-wad)
    add_dependencies(xdre dsda-doom-wad)

    if(NOT WAD_DATA_PATH)
        set(_ACTUAL_WAD_PATH "${CMAKE_BINARY_DIR}/dsda-doom.wad")
    else()
        set(_ACTUAL_WAD_PATH "${WAD_DATA_PATH}")
    endif()

    add_custom_command(TARGET xdre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:xdre>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ACTUAL_WAD_PATH}" "$<TARGET_FILE_DIR:xdre>"
        COMMENT "Copying dsda-doom.wad to xdre target directory"
    )
endif()