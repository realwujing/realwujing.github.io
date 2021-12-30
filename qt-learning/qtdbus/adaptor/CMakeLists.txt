message(${CMAKE_CURRENT_BINARY_DIR})

include_directories(.)
include_directories(../qdbus)

aux_source_directory(. qdbus_adaptor_src)
aux_source_directory(../qdbus qdbus_adaptor_src)

add_executable(adaptor ${qdbus_adaptor_src})

target_link_librarieS(adaptor Qt5::DBus) 