# The libcbe ABI: reconstructed headers, and a link stub to resolve them against.
#
# Shared by every sample under web/, the way media/common is shared by the media
# ones. Nothing here talks to libcbe - it only describes it.
#
# The NDK has no libcbe, so this builds something with the right SONAME to link
# against. libcbe.so carries no version suffix on the TV, and the SONAME is what
# makes the loader pick the real 70 MB library up at runtime, so there is
# deliberately no SOVERSION here. It is never installed.
#
# The stub is written as ordinary C++ against the same headers the samples use,
# rather than as a hand-written list of mangled names, so the two cannot drift.

add_library(cbe-stub SHARED cbe_stub.cpp)
set_target_properties(cbe-stub PROPERTIES OUTPUT_NAME cbe)
target_include_directories(cbe-stub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Chromium is built without RTTI and exports no typeinfo for these classes, so
# anything deriving from them has to agree - hence PUBLIC, not PRIVATE.
target_compile_options(cbe-stub PUBLIC -fno-rtti)

# The same again for webOS 3, where libcbe is a different API and not merely a
# different string ABI: no Initialize(), the constructor takes the size, and the
# delegate has its own slot order. See webos3/webos/webview_base.h.
#
# _GLIBCXX_USE_CXX11_ABI=0 is PUBLIC because it has to reach anything that links
# this - it is what makes std::string mangle the way that generation's libcbe
# expects, and getting it wrong produces a binary that links and then finds
# nothing on the TV.
add_library(cbe-stub-webos3 SHARED webos3/cbe_stub.cpp)
set_target_properties(cbe-stub-webos3 PROPERTIES OUTPUT_NAME cbe)
target_include_directories(cbe-stub-webos3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/webos3")
target_compile_options(cbe-stub-webos3 PUBLIC -fno-rtti)
target_compile_definitions(cbe-stub-webos3 PUBLIC _GLIBCXX_USE_CXX11_ABI=0)

# Both stubs are named libcbe.so, so they need separate build directories.
set_target_properties(cbe-stub-webos3 PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/webos3")
