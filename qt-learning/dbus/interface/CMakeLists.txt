message(${CMAKE_CURRENT_BINARY_DIR})

aux_source_directory(. qdbus_interface_src)

add_executable(interface ${qdbus_interface_src})

target_link_librarieS(interface Qt5::DBus) 