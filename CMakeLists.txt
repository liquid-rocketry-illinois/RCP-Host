cmake_minimum_required(VERSION 3.28)
project(RCP_Controller)

set(CMAKE_C_STANDARD 11)

add_library(RCP-Controller STATIC src/RCP_Controller.c)
target_include_directories(RCP-Controller PUBLIC include/)