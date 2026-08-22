# libcbe - the TV's Chromium, embedded in a native app.
#
# webOS 4 only, and that bound is real rather than cautious. Below it, webOS 3
# ships a libstdc++ without the C++11 std::string ABI that libcbe's own exports
# are mangled with, so nothing can call this API there without statically linking
# a newer runtime. Above it, webOS 5 replaced the free WebOSMain() with a
# webos::WebOSMain class and changed WebViewBase's constructor, and webOS 6 added
# a second, parallel API under neva_app_runtime - so 5 and up want their own
# variant of this sample rather than a wider version range on this one.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# The NDK has no libcbe, so build something with the right SONAME to link against.
# Never installed - the TV's own library is what resolves these at runtime.
# libcbe.so carries no version suffix on the TV, and the SONAME is what makes the
# loader pick the real one up, so it has to match exactly - no SOVERSION here.
add_library(cbe-stub SHARED cbe_stub.cpp)
set_target_properties(cbe-stub PROPERTIES OUTPUT_NAME cbe)
target_include_directories(cbe-stub PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# Chromium is built without RTTI and exports no typeinfo for these classes, so
# anything deriving from them has to agree.
target_compile_options(cbe-stub PRIVATE -fno-rtti)

add_executable(web-cbe main.cpp)
target_include_directories(web-cbe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(web-cbe SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(web-cbe PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_libraries(web-cbe PRIVATE cbe-stub ${GLIB_LIBRARIES})
target_compile_options(web-cbe PRIVATE -fno-rtti)

# No RPATH at all: the stub above must not be shipped, and the TV resolves libcbe
# from /usr/lib like every other system library.
set_target_properties(web-cbe PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET web-cbe
        APPID org.webosbrew.sample.web.cbe
        TITLE "CBE WebView (webOS 4)"
        COLOR "#4285F4"
        WEBOS_VERSIONS ">=4, <5")
