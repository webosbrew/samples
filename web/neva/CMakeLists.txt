# The same web view as web/cbe-webos6, through neva_app_runtime instead.
#
# Both APIs live in the same libcbe on the same TV. This is the one whose
# headers LG published (webosose/chromium87, src/neva/app_runtime/public), which
# makes it the better starting point of the two - see
# ../libcbe/neva/neva_app_runtime/webview_base.h for how far the published order
# can be trusted and how that was checked.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_library(cbe-stub-neva SHARED ../libcbe/neva/cbe_stub.cpp)
set_target_properties(cbe-stub-neva PROPERTIES OUTPUT_NAME cbe
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stub")
target_include_directories(cbe-stub-neva PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../libcbe/neva")
target_compile_options(cbe-stub-neva PUBLIC -fno-rtti)

add_executable(web-neva main.cpp)
target_include_directories(web-neva SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(web-neva PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_libraries(web-neva PRIVATE cbe-stub-neva ${GLIB_LIBRARIES})
target_compile_definitions(web-neva PRIVATE APP_LOG_NAME="web-neva")

set_target_properties(web-neva PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET web-neva
        APPID org.webosbrew.sample.web.neva
        TITLE "Neva WebView (webOS 6+)"
        COLOR "#00897B"
        WEBOS_VERSIONS ">=6.4")
