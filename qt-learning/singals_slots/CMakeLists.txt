cmake_minimum_required(VERSION 3.0.0)
project(singal_slots VERSION 0.1.0)

set(CMAKE_VERBOSE_MAKEFILE ON)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(singals_slots singals_slots.cpp)

target_link_directories(singals_slots PUBLIC .)

target_link_librarieS(singals_slots Qt5::Core) 
