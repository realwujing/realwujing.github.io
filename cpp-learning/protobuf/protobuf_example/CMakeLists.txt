cmake_minimum_required(VERSION 3.0.0)
project(protobuf_example VERSION 0.1.0)

message(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Protobuf REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS nodes.proto)

include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${QT5CORE_INCLUDE_DIRS})

aux_source_directory(. TEST_PROTO_JSON_SRCS)

add_executable(test_proto_json ${TEST_PROTO_JSON_SRCS} ${PROTO_SRCS} ${PROTO_HDRS})

target_link_libraries(test_proto_json ${PROTOBUF_LIBRARIES} Qt5::Core)