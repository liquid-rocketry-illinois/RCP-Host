cmake_minimum_required(VERSION 3.28)
project(RCP-Host)

set(CMAKE_C_STANDARD 11)

add_library(RCP-Host STATIC src/RCP_Host.c)
target_include_directories(RCP-Host PUBLIC include/)

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_custom_target(RCP-Host-GithubRelease COMMAND ${CMAKE_SOURCE_DIR}\\GithubRelease.sh ${CMAKE_BINARY_DIR})
    add_dependencies(RCP-Host-GithubRelease RCP-Host)
endif()