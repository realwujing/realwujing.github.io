cmake_minimum_required(VERSION 3.0.0)
project(ThreadPool VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(ThreadPool main.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -std=c++11 -pthread")
include(CPack)
