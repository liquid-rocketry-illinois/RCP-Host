cmake_minimum_required(VERSION 3.28)
project(RCP-Host)

set(CMAKE_C_STANDARD 11)

add_library(RCP-Host STATIC src/RCP_Host.c)
target_include_directories(RCP-Host PUBLIC include/)