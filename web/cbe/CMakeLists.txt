# The smallest thing that puts a web page on screen: hand the process to
# Chromium, get called back, load a URL.
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

add_executable(web-cbe main.cpp)
target_include_directories(web-cbe SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(web-cbe PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_libraries(web-cbe PRIVATE cbe-stub ${GLIB_LIBRARIES})
target_compile_definitions(web-cbe PRIVATE APP_LOG_NAME="web-cbe")

# No RPATH at all: the link stub must not be shipped, and the TV resolves libcbe
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
