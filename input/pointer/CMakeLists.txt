# Where the pointer actually is, drawn on screen and written to a log.
#
# The Magic Remote cursor lives on an overlay plane the capture service does not
# include, so a screenshot cannot answer "did that wire encoding move the
# pointer, and to where". This sample answers it two ways at once: a crosshair
# whose bar positions are the coordinates, and /tmp/input-pointer.log with the
# exact numbers for every event.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(GLES2 REQUIRED glesv2)

add_executable(input-pointer main.c)
target_include_directories(input-pointer SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS} ${GLES2_INCLUDE_DIRS})
target_link_directories(input-pointer PRIVATE ${SDL2_LIBRARY_DIRS} ${GLES2_LIBRARY_DIRS})
# dl for the webOS-only SDL entry points, looked up by name at runtime.
target_link_libraries(input-pointer PRIVATE ${SDL2_LIBRARIES} ${GLES2_LIBRARIES} ${CMAKE_DL_LIBS})

set_target_properties(input-pointer PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")

webos_add_ipk(
        TARGET input-pointer
        APPID org.webosbrew.sample.input.pointer
        TITLE "Pointer ruler"
        COLOR "#00897B")
