cmake_minimum_required(VERSION 3.0.0)
project(namespace_uts_demo VERSION 0.1.0)

SET(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

add_executable(namespace_uts_demo namespace_uts_demo.c)

add_executable(namespace_join namespace_join.c)

add_executable(namespace_leave namespace_leave.c)