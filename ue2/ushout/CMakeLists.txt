cmake_minimum_required(VERSION 3.7)
project(ex3)

set(CMAKE_C_STANDARD 99)
set(SOURCE_FILES main.c)

set(CMAKE_C_FLAGS "-pthread")
add_executable(main ${SOURCE_FILES})