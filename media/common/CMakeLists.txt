# Backend-agnostic pieces shared by every media sample.
#
# Nothing here may depend on a webOS media API. That keeps the elementary-stream parsers
# buildable and testable on the host, and lets a future media/ndl/ family reuse them
# unchanged.

add_library(media-common STATIC
        app_launch.c
        es_file.c
        json_scan.c
        m3_vdec_fix.c
        pacer.c
        strbuf.c)

target_include_directories(media-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# m3_vdec_fix resolves a platform symbol at runtime.
target_link_libraries(media-common PUBLIC ${CMAKE_DL_LIBS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 sdl2)

if (SDL2_FOUND)
    add_library(media-common-sdl STATIC sdl_shell.c)
    target_include_directories(media-common-sdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(media-common-sdl PUBLIC media-common ${SDL2_LIBRARIES})
    target_link_directories(media-common-sdl PUBLIC ${SDL2_LIBRARY_DIRS})

    # SDL_webOS.h only exists in the webOS SDL fork (the buildroot NDK ships 2.30.12 with
    # it). Everything it guards - the access-policy hints, the remote scancodes, the
    # exported-window API - is absent from stock SDL, so it is probed rather than assumed.
    include(CheckIncludeFile)
    include(CMakePushCheckState)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_INCLUDES ${SDL2_INCLUDE_DIRS})
    check_include_file("SDL_webOS.h" HAVE_SDL_WEBOS)
    cmake_pop_check_state()

    if (HAVE_SDL_WEBOS)
        target_compile_definitions(media-common-sdl PUBLIC HAVE_SDL_WEBOS=1)
    endif ()
endif ()

if (BUILD_TESTING OR NOT TARGET_WEBOS)
    add_executable(es-dump tools/es_dump.c)
    target_link_libraries(es-dump PRIVATE media-common)
endif ()
