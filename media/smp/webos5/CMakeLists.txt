# webOS 5 and newer: SDL exported window, id carried in the Load payload.
#
# This variant needs no compile definitions - modern __cxx11 ABI, Load() with a user
# pointer, constructor taking a uid - so it links the plain `modern` common library.

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

if (NOT HAVE_SDL_WEBOS)
    message(WARNING "Skipping media-smp-webos5: SDL_webOS.h not found. "
            "The exported-window API only exists in the webOS SDL fork "
            "(webosbrew/SDL-webOS); the buildroot NDK ships it.")
    return()
endif ()

# SDL2 is NOT bundled - see the note in ../acb/CMakeLists.txt. The -verify target is what
# establishes whether the firmware's own SDL exports the exported-window API this sample
# needs.

add_executable(media-smp-webos5
        main.c
        exported_window.c)

target_link_libraries(media-smp-webos5 PRIVATE
        media-smp-common-modern
        media-common-sdl)

set_target_properties(media-smp-webos5 PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE)

webos_add_ipk(
        TARGET media-smp-webos5
        APPID org.webosbrew.sample.media.smp.webos5
        TITLE "SMP Exported Window (webOS 5+)"
        # The ABI here is fine on webOS 4 as well - what makes this a 5+ sample is the
        # exported-window API, which webOS 4 has no equivalent of.
        COLOR "#00BCD4"
        WEBOS_VERSIONS ">=5"
        MEDIA "${SAMPLE_VIDEO_ES}" "${SAMPLE_AUDIO_ES}")
