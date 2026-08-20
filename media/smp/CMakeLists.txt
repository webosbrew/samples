# starfish-media-pipeline samples.
#
# One directory per way of getting the decoded picture onto the screen. The webOS version
# differences that are *not* about the video plane - the C++ ABI, the Load() overload - are
# build variants of common/ instead, so they do not multiply directories.

add_subdirectory(common)
add_subdirectory(acb)
add_subdirectory(webos5)
