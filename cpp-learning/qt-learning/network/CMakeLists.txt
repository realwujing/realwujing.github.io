cmake_minimum_required(VERSION 3.0.0)
project(network VERSION 0.1.0)

# set(CMAKE_VERBOSE_MAKEFILE ON)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_AUTOMOC ON)

set(CMAKE_BUILD_TYPE "Debug")

find_package(Qt5 REQUIRED COMPONENTS Core Network)

add_executable(network main.cpp worker.cpp)

target_link_libraries(network Qt5::Core Qt5::Network) 
