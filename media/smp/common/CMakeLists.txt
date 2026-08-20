# Shared plumbing for every starfish-media-pipeline sample.
#
# The same three sources are compiled once per webOS ABI generation. Nothing else in the
# repo is built more than once - this is where the version differences live, and keeping
# them in a build file rather than in #ifdef soup is the point.
#
#   variant                            std::string ABI  Load()   ctor            used by
#   media-smp-common-modern            __cxx11          4-arg    (const char *)  webOS 4, 5+
#   media-smp-common-legacy            pre-C++11        4-arg    (const char *)  webOS 3
#   media-smp-common-legacy-nouserdata pre-C++11        3-arg    (const char *)  webOS 2
#   media-smp-common-webos1            pre-C++11        3-arg    ()              webOS 1
#
# Verified by diffing exported symbols of libplayerAPIs.so.1.0.0 across retail firmware:
# ...4FeedB5cxx11EPKc appears in 2018 (webOS 4.0) images and later, ...4FeedEPKc in 2017
# (webOS 3.5) and earlier; ...4LoadEPKcPFvixS1_PvES2_ is absent before webOS 3;
# ...C1Ev exists only on webOS 1.

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPLAYERAPIS REQUIRED libplayerAPIs)

function(smp_add_common NAME)
    cmake_parse_arguments(SMP "LEGACY_ABI;NO_LOAD_USERDATA;CTOR_NO_UID" "" "" ${ARGN})

    add_library(${NAME} STATIC smp_api.cpp smp_payload.c smp_player.c)
    target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            SYSTEM PUBLIC ${LIBPLAYERAPIS_INCLUDE_DIRS})
    target_link_directories(${NAME} PUBLIC ${LIBPLAYERAPIS_LIBRARY_DIRS})
    # stdc++ explicitly: the samples are C programs that happen to contain one C++
    # translation unit, so the C driver does the linking and will not add it by itself.
    target_link_libraries(${NAME} PUBLIC media-common ${LIBPLAYERAPIS_LIBRARIES} stdc++ pthread)

    if (SMP_LEGACY_ABI)
        # webOS 3 and older ship libstdc++ 6.0.19, built before the std::string ABI change.
        # Feed() returns a std::string by value, so this has to match the TV or the process
        # crashes on the first feed.
        target_compile_definitions(${NAME} PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
    endif ()
    if (SMP_NO_LOAD_USERDATA)
        target_compile_definitions(${NAME} PRIVATE SMP_LOAD_HAS_USERDATA=0)
    endif ()
    if (SMP_CTOR_NO_UID)
        target_compile_definitions(${NAME} PRIVATE SMP_CTOR_NO_UID=1)
    endif ()
endfunction()

smp_add_common(media-smp-common-modern)
smp_add_common(media-smp-common-legacy LEGACY_ABI)
smp_add_common(media-smp-common-legacy-nouserdata LEGACY_ABI NO_LOAD_USERDATA)
smp_add_common(media-smp-common-webos1 LEGACY_ABI NO_LOAD_USERDATA CTOR_NO_UID)
