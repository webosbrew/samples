# LGNC (LG NetCast Open API) - webOS 1 through 4.
#
# Unlike media/smp and media/ndl there is only one sample here and no build variants: the
# function set this uses is identical on every retail firmware from webOS 1 to webOS 4, so
# one binary covers the lot - including webOS 1, which neither of the other families
# reaches without special handling.
#
# The upper bound is deliberately 5, not 6. webOS 5 does still export the whole API, so a
# build targeting it links and passes -verify; it just does not work. Only webOS 6 drops
# the library. This is the one place in the repo where the symbol tables are not the last
# word - see the note in lgnc_player.h.

find_package(PkgConfig REQUIRED)
pkg_check_modules(LGNC REQUIRED lgncopenapi)

add_executable(media-lgnc
        main.c
        lgnc_player.c)

target_include_directories(media-lgnc SYSTEM PRIVATE ${LGNC_INCLUDE_DIRS})
target_link_directories(media-lgnc PRIVATE ${LGNC_LIBRARY_DIRS})
target_link_libraries(media-lgnc PRIVATE
        media-common
        media-common-sdl
        ${LGNC_LIBRARIES})

set_target_properties(media-lgnc PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE)

webos_add_ipk(
        TARGET media-lgnc
        APPID org.webosbrew.sample.media.lgnc
        TITLE "LGNC Player (webOS 1-4)"
        COLOR "#FF9800"
        WEBOS_VERSIONS ">=1, <5"
        MEDIA "${SAMPLE_VIDEO_ES}" "${SAMPLE_AUDIO_ES}")
