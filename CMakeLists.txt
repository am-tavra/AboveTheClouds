cmake_minimum_required(VERSION 3.20)
project(AboveTheClouds C)

set(CMAKE_C_STANDARD 11)

set(RAYLIB_PREFIX "/opt/homebrew")

add_executable(above_the_clouds src/main.c)

target_include_directories(above_the_clouds PRIVATE ${RAYLIB_PREFIX}/include)
target_link_directories(above_the_clouds PRIVATE ${RAYLIB_PREFIX}/lib)
target_link_libraries(above_the_clouds raylib
    "-framework IOKit"
    "-framework Cocoa"
    "-framework OpenGL"
)
