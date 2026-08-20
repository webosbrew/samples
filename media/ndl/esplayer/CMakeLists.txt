# webOS 2.x - 3.4: NDL Esplayer, from libndl-directmedia2.
#
# Neither the headers nor a link stub are in the buildroot NDK, so they come from
# webosbrew/webos-userland - see cmake/WebOSUserland.cmake. Nothing is vendored into this
# repo and nothing is bundled into the ipk: the real library is already on the TV.

webos_userland_provide(libndl-directmedia2 NDL_DM2_LIBRARIES NDL_DM2_INCLUDE_DIRS)

add_executable(media-ndl-esplayer
        main.c
        esplayer.c)

target_include_directories(media-ndl-esplayer SYSTEM PRIVATE ${NDL_DM2_INCLUDE_DIRS})
target_link_libraries(media-ndl-esplayer PRIVATE
        media-common
        media-common-sdl
        ${NDL_DM2_LIBRARIES})

set_target_properties(media-ndl-esplayer PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE)

webos_add_ipk(
        TARGET media-ndl-esplayer
        APPID org.webosbrew.sample.media.ndl.esplayer
        TITLE "NDL Esplayer (webOS 2-3.4)"
        # libndl-directmedia2 is gone from webOS 3.5 on, replaced by libNDL_directmedia
        # with an entirely different API.
        ICON_COLOR "#4CAF50"
        WEBOS_VERSIONS ">=2, <3.5"
        MEDIA "${SAMPLE_VIDEO_ES}" "${SAMPLE_AUDIO_ES}")
