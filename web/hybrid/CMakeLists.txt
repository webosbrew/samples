# SDL2 and a libcbe web view in one process, swapping which one is on screen.
#
# The interesting constraint is in main.cpp: WebOSMain() owns the process and the
# message loop, so SDL cannot have its own. It is pumped from the glib loop that
# libcbe already runs on its browser UI thread, which puts both toolkits on one
# thread and makes switching views a plain function call.
#
# webOS 4 only, for the same ABI reasons as web/cbe.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(web-hybrid main.cpp)
target_include_directories(web-hybrid SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_directories(web-hybrid PRIVATE ${GLIB_LIBRARY_DIRS} ${SDL2_LIBRARY_DIRS})
target_link_libraries(web-hybrid PRIVATE cbe-stub ${GLIB_LIBRARIES} ${SDL2_LIBRARIES})
target_compile_definitions(web-hybrid PRIVATE APP_LOG_NAME="web-hybrid")

set_target_properties(web-hybrid PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET web-hybrid
        APPID org.webosbrew.sample.web.hybrid
        TITLE "SDL + WebView (webOS 4)"
        COLOR "#7E57C2"
        WEBOS_VERSIONS ">=4, <5"
        MEDIA "${CMAKE_CURRENT_SOURCE_DIR}/page.html")
