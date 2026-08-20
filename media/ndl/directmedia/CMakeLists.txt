# webOS 3.5+: NDL DirectMedia, from libNDL_directmedia (which the NDK does ship).
#
# One main.c, one facade header, two implementations - because the SDK header is really two
# different APIs behind NDL_DIRECTMEDIA_API_VERSION:
#
#   v1  webOS 3.5 - 4.x   per-stream Open/Play/Close, no timestamps, AAC audio
#   v2  webOS 5+          combined Load with a callback, timestamps, no AAC
#
# The split point is exactly webOS 5, matching how ss4s gates its own two NDL modules.

find_package(PkgConfig REQUIRED)
pkg_check_modules(NDL_DIRECTMEDIA REQUIRED NDL_directmedia)

# NAME is the API version, IMPL its source, MEDIA_AUDIO the audio file that version can
# actually play, VERSIONS the firmware range for the symbol check.
function(directmedia_add_sample NAME IMPL MEDIA_AUDIO VERSIONS)
    set(target media-ndl-directmedia-${NAME})

    add_executable(${target} main.c ${IMPL})
    target_include_directories(${target} SYSTEM PRIVATE ${NDL_DIRECTMEDIA_INCLUDE_DIRS})
    target_link_directories(${target} PRIVATE ${NDL_DIRECTMEDIA_LIBRARY_DIRS})
    target_link_libraries(${target} PRIVATE
            media-common media-common-sdl ${NDL_DIRECTMEDIA_LIBRARIES})
    # The headers deprecate their own legacy typedefs, and the structs still use them.
    target_compile_options(${target} PRIVATE -Wno-error=deprecated-declarations)
    set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE)

    webos_add_ipk(
            TARGET ${target}
            APPID org.webosbrew.sample.media.ndl.directmedia.${NAME}
            TITLE "NDL DirectMedia ${NAME}"
            WEBOS_VERSIONS "${VERSIONS}"
            MEDIA "${SAMPLE_VIDEO_ES}" "${MEDIA_AUDIO}")
endfunction()

directmedia_add_sample(v1 directmedia_v1.c "${SAMPLE_AUDIO_ES}" ">=3.5, <5")
directmedia_add_sample(v2 directmedia_v2.c "${SAMPLE_AUDIO_PCM}" ">=5")
