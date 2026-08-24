# web/cbe, built against the libcbe that ships on webOS 6 and newer.
#
# One binary for 6.4 through 11.2: the webos:: API stops moving after 6.4, so
# unlike the webOS 3 and 4 generations this range needs no further variants.
# WAM still subclasses this API on these sets, which is what made the 61-slot
# delegate recoverable - see ../libcbe/webos6/webos/webview_base.h.
#
# Verified against a 65UP7560 running starfish 6.5.2.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_library(cbe-stub-webos6 SHARED ../libcbe/webos6/cbe_stub.cpp)
set_target_properties(cbe-stub-webos6 PROPERTIES OUTPUT_NAME cbe
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stub")
target_include_directories(cbe-stub-webos6 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../libcbe/webos6")
target_compile_options(cbe-stub-webos6 PUBLIC -fno-rtti)

add_executable(web-cbe-webos6 main.cpp)
target_include_directories(web-cbe-webos6 SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(web-cbe-webos6 PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_libraries(web-cbe-webos6 PRIVATE cbe-stub-webos6 ${GLIB_LIBRARIES})
target_compile_definitions(web-cbe-webos6 PRIVATE APP_LOG_NAME="web-cbe-webos6")

set_target_properties(web-cbe-webos6 PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET web-cbe-webos6
        APPID org.webosbrew.sample.web.cbe6
        TITLE "CBE WebView (webOS 6+)"
        COLOR "#5C6BC0"
        WEBOS_VERSIONS ">=6, <7")
