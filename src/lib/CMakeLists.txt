cmake_minimum_required(VERSION 3.17)
project(udp_client C)

set(CMAKE_C_STANDARD 11)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    message("no debug")
    set(isDebug FALSE)
endif()


if (isDebug)
        message("Debug build detected")
        add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
        set(CONFIGURATION_DEBUG 1)
endif()


set(DEPS ../../deps/)


file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS
        "${deps}*/src/lib/*.c"
        )

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
        "*.c"
        )

add_library(udp_client
    ${lib_src}
)

target_include_directories(udp_client PUBLIC ${DEPS}piot/clog/src/include)
target_include_directories(udp_client PUBLIC ${DEPS}piot/tiny-libc/src/include)
target_include_directories(udp_client PUBLIC ../include)

