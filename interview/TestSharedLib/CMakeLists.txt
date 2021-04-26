cmake_minimum_required(VERSION 3.10)
project(TestSharedLib)

# C++11 编译
set(CMAKE_CXX_STANDARD 11)

# 头文件路径
# set(INC_DIR /home/xx/code/clion/MySharedLib)
set(INC_DIR /home/wujing/code/CPlusPlusProject/interview/MySharedLib)
# 库文件路径
# set(LIB_DIR /home/xx/code/clion/MySharedLib/cmake-build-debug)
set(LIB_DIR /home/wujing/code/CPlusPlusProject/interview/MySharedLib/build/)

# 可执行文件放置于源代码路径/bin
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${INC_DIR})
link_directories(${LIB_DIR})
link_libraries(MySharedLib)

add_executable(TestSharedLib main.cpp)

# 链接 MySharedLib 库
target_link_libraries(TestSharedLib MySharedLib)