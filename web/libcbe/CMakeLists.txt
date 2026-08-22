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
