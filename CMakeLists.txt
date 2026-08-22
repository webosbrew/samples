cmake_minimum_required(VERSION 3.18)
project(webos-samples LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(WebOSTarget)
include(WebOSPackage)
include(WebOSUserland)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# C++11 only, and only for the StarfishMediaAPIs shim - webOS 3 and older ship
# libstdc++ 6.0.19 (GCC 4.8), so anything newer risks unresolvable symbols on device.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall -Wextra)

# The sample streams are generated, not committed. Override to use your own.
set(SAMPLE_MEDIA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH
        "Directory holding sample.h264 and sample.aac")

set(SAMPLE_VIDEO_ES "${SAMPLE_MEDIA_DIR}/sample.h264")
set(SAMPLE_AUDIO_ES "${SAMPLE_MEDIA_DIR}/sample.aac")
# NDL DirectMedia v2 (webOS 5+) has no AAC in its audio types, so that sample plays raw PCM.
set(SAMPLE_AUDIO_PCM "${SAMPLE_MEDIA_DIR}/sample.pcm")
set(SAMPLE_AUDIO_AC3 "${SAMPLE_MEDIA_DIR}/sample.ac3")

add_subdirectory(media/common)

if (TARGET_WEBOS)
    add_subdirectory(media/smp)
    add_subdirectory(media/ndl)
    add_subdirectory(media/lgnc)
    add_subdirectory(web/libcbe)
    add_subdirectory(web/cbe)
    add_subdirectory(web/hybrid)
endif ()
