cmake_minimum_required(VERSION 3.16)
project(C__Project)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(C__Project
        main.cpp
        name.cpp
        name.h Box.cpp Box.h Shape.cpp Shape.h Rectangle.cpp Rectangle.h)
