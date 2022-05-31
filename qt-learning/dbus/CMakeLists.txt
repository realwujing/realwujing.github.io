cmake_minimum_required(VERSION 3.0.0)
project(QtDBus VERSION 0.1.0)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(CMAKE_BUILD_TYPE DEBUG)
# ADD_DEFINITIONS(-DQT_MESSAGELOGCONTEXT)
# add_compile_definitions(QT_MESSAGELOGCONTEXT)

find_package(Qt5 REQUIRED COMPONENTS Core DBus)

message(${Qt5DBus_INCLUDE_DIRS})
message(${CMAKE_CURRENT_LIST_LINE})
foreach(dir ${Qt5DBus_INCLUDE_DIRS})
  message(STATUS "dir='${dir}'")
endforeach()
message(${CMAKE_CURRENT_LIST_LINE})
message(${Qt5DBus_LIBRARIES})

add_subdirectory(qdbus)

add_subdirectory(service)

add_subdirectory(message)

add_subdirectory(interface)

add_subdirectory(proxy)

add_subdirectory(adaptor)

# add_subdirectory(test_dbus_module)