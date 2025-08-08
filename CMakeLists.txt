cmake_minimum_required(VERSION 3.28)
project(RCP-Host)

set(CMAKE_C_STANDARD 11)

add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/VERSION.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/VERSION
        COMMAND ${CMAKE_COMMAND} -DSOURCE:STRING=${CMAKE_SOURCE_DIR} -DBIN:STRING=${CMAKE_BINARY_DIR}
        -DBTYPE:STRING=${CMAKE_BUILD_TYPE} -P ${CMAKE_SOURCE_DIR}/cmake/gen_version.cmake
)

add_library(RCP-Host STATIC src/RCP_Host.c ${CMAKE_BINARY_DIR}/VERSION.cpp)
target_include_directories(RCP-Host PUBLIC include/)

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_custom_target(RCP-Host-GithubRelease COMMAND ${CMAKE_SOURCE_DIR}\\GithubRelease.sh ${CMAKE_BINARY_DIR})
    add_dependencies(RCP-Host-GithubRelease RCP-Host)
endif()