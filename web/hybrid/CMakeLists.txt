# SDL2 and a libcbe web view in one process, swapping which one is on screen.
#
# The interesting constraint is in main.cpp: WebOSMain() owns the process and the
# message loop, so SDL cannot have its own. It is pumped from the glib loop that
# libcbe already runs on its browser UI thread, which puts both toolkits on one
# thread and makes switching views a plain function call.
#
# webOS 4 only, for the same ABI reasons as web/cbe - and verified only from 4.4.2
# up, since no earlier webOS 4 dump exists.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(GLES2 REQUIRED glesv2)

# Nuklear draws the native view. It is two single headers - the library and its
# SDL/GLES2 backend - so it is fetched rather than vendored, and compiled into
# exactly one translation unit (native_ui.c).
#
# GLES2 and not SDL_Renderer, and that is forced: the TV ships SDL 2.0.4, which
# predates SDL_RenderGeometry, so Nuklear's sdl_renderer backend cannot link
# there. The buildroot NDK's SDL is 2.30.12, so this is exactly the kind of
# mistake that builds cleanly and fails on the device - `-verify` is what
# catches it.
include(FetchContent)
set(NUKLEAR_REPOSITORY "https://github.com/Immediate-Mode-UI/Nuklear.git"
        CACHE STRING "Nuklear git repository")
set(NUKLEAR_TAG "master" CACHE STRING "Nuklear git tag or branch")
FetchContent_Declare(nuklear
        GIT_REPOSITORY "${NUKLEAR_REPOSITORY}"
        GIT_TAG "${NUKLEAR_TAG}"
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add-this-project)
FetchContent_MakeAvailable(nuklear)

add_executable(web-hybrid main.cpp native_ui.c)
target_include_directories(web-hybrid PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(web-hybrid SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS}
        "${nuklear_SOURCE_DIR}" "${nuklear_SOURCE_DIR}/demo/sdl_opengles2")
target_link_directories(web-hybrid PRIVATE ${GLIB_LIBRARY_DIRS} ${SDL2_LIBRARY_DIRS} ${GLES2_LIBRARY_DIRS})
target_link_libraries(web-hybrid PRIVATE cbe-stub ${GLIB_LIBRARIES} ${SDL2_LIBRARIES} ${GLES2_LIBRARIES})
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
        WEBOS_VERSIONS ">=4, <4.10"
        MEDIA "${CMAKE_CURRENT_SOURCE_DIR}/page.html")
