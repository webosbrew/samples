# webOS 2 through 4.x: libAcbAPI drives the video plane.
#
# One source, three ipks. The video plane is identical across these generations; what
# differs is only the libplayerAPIs C++ ABI and which Load() overload exists, so each
# target links a different build of media/smp/common. See that directory for the matrix.

find_package(PkgConfig REQUIRED)
pkg_check_modules(ACBAPI REQUIRED AcbAPI)
pkg_check_modules(SDL2 REQUIRED sdl2)

# SDL2 is NOT bundled. Every webOS TV already ships one, and the NDK's much newer build
# expects a newer libwayland than older firmware has - vendoring it produces an app that
# links fine and then fails at SDL_INIT_VIDEO on the device. Link the system SDL instead
# and let the -verify target prove, per firmware, that the symbols used are really there.

# NAME is the webOS generation; COMMON is the ABI variant it needs; VERSIONS scopes the
# symbol check to the firmware releases that variant is built for.
function(acb_add_sample NAME COMMON VERSIONS)
    set(target media-smp-acb-${NAME})

    add_executable(${target} main.c acb_plane.c)
    if (NAME STREQUAL "webos2")
        # webOS 2.2 delivers LOADCOMPLETED only once feeding stops, so the sink has to be
        # attached as soon as Load() returns. Every later generation rejects that - see
        # attach_sink() in acb_plane.c.
        target_compile_definitions(${target} PRIVATE ACB_ATTACH_SINK_EARLY=1)
    endif ()
    target_include_directories(${target} SYSTEM PRIVATE ${ACBAPI_INCLUDE_DIRS})
    target_link_directories(${target} PRIVATE ${ACBAPI_LIBRARY_DIRS})
    target_link_libraries(${target} PRIVATE ${COMMON} media-common-sdl ${ACBAPI_LIBRARIES})
    set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE)

    webos_add_ipk(
            TARGET ${target}
            APPID org.webosbrew.sample.media.smp.acb.${NAME}
            TITLE "SMP Player (${NAME})"
            WEBOS_VERSIONS "${VERSIONS}"
            MEDIA "${SAMPLE_VIDEO_ES}" "${SAMPLE_AUDIO_ES}")
endfunction()

acb_add_sample(webos2 media-smp-common-legacy-nouserdata ">=2, <3")
acb_add_sample(webos3 media-smp-common-legacy ">=3, <4")
acb_add_sample(webos4 media-smp-common-modern ">=4, <5")
