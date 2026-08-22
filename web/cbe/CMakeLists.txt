# The smallest thing that puts a web page on screen: hand the process to
# Chromium, get called back, load a URL.
#
# webOS 4 only, and both bounds are build variants rather than walls - the same
# situation as media/smp/common, which is built four times for four generations.
# Below, webOS 3's libcbe is the pre-C++11 std::string ABI throughout and predates
# the WebViewBase() + Initialize() split, so it wants a legacy variant. Above,
# webOS 5 replaced the free WebOSMain() with a webos::WebOSMain class and changed
# the constructor again. The hard floor is 3.4: webOS 1 and 2 have no libcbe.
#
# The upper bound is starfish 4.9, not 5. The firmware dumps carry one entry per
# major release, and webOS 4 has two of them: starfish 4.4.2 is webOS 4.0 (the
# 2018 sets, which this targets) and starfish 4.10.0 is webOS 4.5 (2019), where
# Initialize() takes a ninth argument. -verify does not catch that - it reports
# All OK for 4.10.0 despite the symbol being absent from the whole dump - so this
# range comes from reading the symbol tables. See README.md.

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
        WEBOS_VERSIONS ">=4, <4.10")
