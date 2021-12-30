message(${CMAKE_CURRENT_BINARY_DIR})

include_directories(../qdbus)

aux_source_directory(. qdbus_service_srcs)
aux_source_directory(../qdbus qdbus_service_srcs)

add_executable(service ${qdbus_service_srcs})

target_link_librarieS(service Qt5::DBus) 