# Provides the webOS libraries that the buildroot NDK does not ship.
#
# libndl-directmedia2 is the webOS 2.x - 3.4 media API. It exists on those TVs, but the SDK
# has neither its headers nor a link stub, so both are taken from webosbrew/webos-userland -
# the same project that produces the SDK's stubs for libplayerAPIs and libAcbAPI.
#
#   webos_userland_provide(<pkg-config name> <target var> <include var>)
#
# pkg-config wins if the SDK does have the library, so a newer NDK makes the fetch
# unnecessary without any change here.

include(FetchContent)

set(WEBOS_USERLAND_REPOSITORY "https://github.com/webosbrew/webos-userland.git"
        CACHE STRING "webos-userland git repository")
set(WEBOS_USERLAND_TAG "main" CACHE STRING "webos-userland git tag or branch")

# Only the sources are wanted, never webos-userland's own CMakeLists: it builds stubs for
# everything (libplayerAPIs, libAcbAPI, EGL...) which would shadow the real SDK ones, and
# it resolves its linker scripts through CMAKE_SOURCE_DIR, which is wrong for a subproject.
# SOURCE_SUBDIR pointing at a directory that does not exist is the documented way to
# populate without adding the project.
function(_webos_userland_populate)
    if (webos_userland_POPULATED)
        return()
    endif ()
    message(STATUS "Fetching webos-userland (libraries missing from the NDK)")
    FetchContent_Declare(webos_userland
            GIT_REPOSITORY "${WEBOS_USERLAND_REPOSITORY}"
            GIT_TAG "${WEBOS_USERLAND_TAG}"
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR do-not-add-this-project)
    FetchContent_MakeAvailable(webos_userland)
    set(webos_userland_SOURCE_DIR "${webos_userland_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# Builds a link-time stub: every exported name resolves to the same dummy function, and the
# SONAME is what makes the loader pick up the TV's real library at runtime. Exactly what
# the SDK's own libplayerAPIs.so is.
function(_webos_userland_add_stub NAME SOVERSION)
    if (TARGET ${NAME})
        return()
    endif ()
    add_library(${NAME} SHARED "${webos_userland_SOURCE_DIR}/src/dummy.c")
    set_target_properties(${NAME} PROPERTIES SOVERSION ${SOVERSION})
    target_link_options(${NAME} PRIVATE "${webos_userland_SOURCE_DIR}/src/${NAME}.lds")
endfunction()

function(webos_userland_provide PKG_NAME OUT_TARGET OUT_INCLUDE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(_WU_${PKG_NAME} ${PKG_NAME} QUIET)

    if (_WU_${PKG_NAME}_FOUND)
        set(${OUT_TARGET} "${_WU_${PKG_NAME}_LIBRARIES}" PARENT_SCOPE)
        set(${OUT_INCLUDE} "${_WU_${PKG_NAME}_INCLUDE_DIRS}" PARENT_SCOPE)
        return()
    endif ()

    if (NOT PKG_NAME STREQUAL "libndl-directmedia2")
        message(FATAL_ERROR "webos_userland_provide: don't know how to provide ${PKG_NAME}")
    endif ()

    _webos_userland_populate()
    _webos_userland_add_stub(ndl-directmedia2 1)

    set(${OUT_TARGET} ndl-directmedia2 PARENT_SCOPE)
    set(${OUT_INCLUDE} "${webos_userland_SOURCE_DIR}/include" PARENT_SCOPE)
endfunction()
