# Absolute pointer placement, via a uinput device libim will accept.
# No SDL: this is a plain evdev producer. See main.c for why both the device
# name and IM_RequestToCheckInput are required.
add_executable(input-abspointer main.c)
target_link_libraries(input-abspointer PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(input-abspointer PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "")
