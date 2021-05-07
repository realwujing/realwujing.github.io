cmake_minimum_required(VERSION 3.0.0)
project(Log VERSION 0.1.0)

include(CTest)
enable_testing()

aux_source_directory(. SRC_LIST) #搜索当前目录下的所有.cpp文件
add_executable(Log ${SRC_LIST})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
