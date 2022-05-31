cmake_minimum_required(VERSION 3.0.0)
project(qprocess_wget VERSION 0.1.0)

# set(CMAKE_VERBOSE_MAKEFILE ON)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_AUTOMOC ON)

set(CMAKE_BUILD_TYPE "Debug")

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(qprocess_wget main.cpp wget.cpp)

target_link_libraries(qprocess_wget Qt5::Core) 
