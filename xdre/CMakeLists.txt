cmake_minimum_required(VERSION 3.20)
project(xdre C CXX)

# opengl for windows only, sdl2 for all other platforms
find_package(SDL2 REQUIRED)
if(WIN32)
    find_package(OpenGL REQUIRED)
endif()

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

    # ImGui SDL2 backend
    imgui/backends/imgui_impl_sdl2.cpp
)

# opengl backend for windows, sdl2 for other platforms
if(WIN32)
    list(APPEND XDRE_SOURCES imgui/backends/imgui_impl_opengl3.cpp)
else()
    list(APPEND XDRE_SOURCES imgui/backends/imgui_impl_sdlrenderer2.cpp)
endif()

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
    imgui
    imgui/backends
    ${XDRE_EXPANDED_SDL_INCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsda-doom/prboom2/src
    ${CMAKE_CURRENT_BINARY_DIR}/../dsda-doom/build-config
)

target_compile_definitions(xdre PRIVATE
    XDRE=1
    PACKAGE_STRING="LDRE beta v0.6"
    HAVE_CONFIG_H
)

# distinctive compiler flags for platforms
if(MSVC)
    target_compile_definitions(xdre PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(xdre PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/O2 /Gw /Gy>
        ${LDRE_ARCH_FLAGS}
        /W3
        /wd4996
        /wd4244
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(xdre PRIVATE
            /clang:-fno-strict-aliasing
            /clang:-fwrapv
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
            $<$<NOT:$<CONFIG:Debug>>:-O2 -ftree-vectorize -fomit-frame-pointer>
            -g
            ${LDRE_ARCH_FLAGS}
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

if(LDRE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok)
    if(_ipo_ok)
        set_property(TARGET xdre PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        foreach(_t dsda-doom-core dsda-doom dsda-doom-core-xdre)
            if(TARGET ${_t})
                set_property(TARGET ${_t} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
            endif()
        endforeach()
    endif()
endif()

target_link_libraries(xdre PRIVATE
    dsda-doom-core-xdre
    ${XDRE_SDL2_TARGET}
)

if(WIN32)
    target_link_libraries(xdre PRIVATE
        OpenGL::GL
        ws2_32
        winmm
        imm32
        comdlg32
        version
        setupapi
    )
    if(MSVC)
        set_target_properties(xdre PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF")
    else()
        set_target_properties(xdre PROPERTIES LINK_FLAGS "-mconsole")
    endif()
endif()

# build and copy standalone dsda-doom executable alongside xdre for brute forcing
if(TARGET dsda-doom)
    add_dependencies(xdre dsda-doom)
    add_custom_command(TARGET xdre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:xdre>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:dsda-doom>"
            "$<TARGET_FILE_DIR:xdre>/$<TARGET_FILE_NAME:dsda-doom>"
        COMMENT "Copying dsda-doom executable to xdre target directory"
    )
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
