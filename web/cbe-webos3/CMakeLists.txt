# web/cbe, built against webOS 3's libcbe.
#
# Not a flag flip. Between webOS 3 and 4 the library changed its string ABI *and*
# its API: WebViewBase takes its size in the constructor with no Initialize(),
# WebAppWindowBase has no InitWindow(), and the delegate has a different slot
# order with LoadStarted carrying a URL where webOS 4 grew a separate
# DidStartNavigation. So this is a sibling sample rather than a build variant,
# the way media/smp/acb and media/smp/webos5 are siblings.
#
# Verified against a 43UH6100 running starfish 3.4.0.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_executable(web-cbe-webos3 main.cpp luna_register.c)
target_include_directories(web-cbe-webos3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(web-cbe-webos3 SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(web-cbe-webos3 PRIVATE ${GLIB_LIBRARY_DIRS})
# cbe-stub-webos3 carries the include path, -fno-rtti and the old string ABI.
target_link_libraries(web-cbe-webos3 PRIVATE cbe-stub-webos3 ${GLIB_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_definitions(web-cbe-webos3 PRIVATE APP_LOG_NAME="web-cbe-webos3")

set_target_properties(web-cbe-webos3 PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET web-cbe-webos3
        APPID org.webosbrew.sample.web.cbe3
        TITLE "CBE WebView (webOS 3)"
        COLOR "#26A69A"
        WEBOS_VERSIONS ">=3.4, <4")
