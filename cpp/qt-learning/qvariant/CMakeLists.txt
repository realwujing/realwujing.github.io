cmake_minimum_required(VERSION 3.5)
project(MyQtApp VERSION 0.0.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# package program
message(STATUS "CPack Configuration added.")

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR})
set(CPACK_SOURCE_IGNORE_FILES build)

set(CPACK_PACKAGE_CONTACT "xxx@163.com")
set(CPACK_PACKAGE_DESCRIPTION "cpack test program")

# 将 example_dependency 库添加到 CPack 的依赖项中
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qtbase5-dev (>= 5.15.7.2-1+dde)")

find_package(Qt5 COMPONENTS Core REQUIRED)

# 设置构建类型为Debug
set(CMAKE_BUILD_TYPE Debug)

set(SOURCES
    main.cpp
)

add_executable(MyQtApp ${SOURCES})

target_link_libraries(MyQtApp PRIVATE Qt5::Core)

install(TARGETS MyQtApp
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# according to CMake, CPack must be included LAST
# after all settings have been set
include(CPack)
