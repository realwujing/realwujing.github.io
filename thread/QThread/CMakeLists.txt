cmake_minimum_required(VERSION 3.0.0)
project(QtDBus VERSION 0.1.0)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -std=c++11 -pthread")
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
# ADD_DEFINITIONS(-DQT_MESSAGELOGCONTEXT)
# add_compile_definitions(QT_MESSAGELOGCONTEXT)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(job main.cpp job.cpp job_manager.cpp)

target_link_libraries(job Qt5::Core)




