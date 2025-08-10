cmake_minimum_required(VERSION 3.28)
project(RCP-Host)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

option(BUILD_TESTS "Build GTest RCP tests" OFF)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/VERSION.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
        COMMAND ${CMAKE_COMMAND} -DSOURCE:STRING=${CMAKE_CURRENT_SOURCE_DIR} -DBIN:STRING=${CMAKE_CURRENT_BINARY_DIR}
        -DBTYPE:STRING=${CMAKE_BUILD_TYPE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_version.cmake
)

add_library(RCP-Host STATIC src/RCP_Host.c ${CMAKE_CURRENT_BINARY_DIR}/VERSION.cpp)
target_include_directories(RCP-Host PUBLIC include/)

target_compile_options(RCP-Host PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W3 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

if(${BUILD_TESTS})
    add_subdirectory(test/googletest)

    enable_testing()

    add_executable(
            RCP-Host-tests
            test/test.cpp
    )

    target_link_libraries(RCP-Host-tests PRIVATE GTest::gtest_main RCP-Host)

    include(GoogleTest)
    gtest_discover_tests(RCP-Host-tests)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_custom_target(RCP-Host-GithubRelease COMMAND ${CMAKE_CURRENT_SOURCE_DIR}\\GithubRelease.sh ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    add_dependencies(RCP-Host-GithubRelease RCP-Host)

    if(${BUILD_TESTS})
        add_dependencies(RCP-Host-GithubRelease RCP-Host-tests)
    endif()
endif()