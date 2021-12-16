message(${CMAKE_CURRENT_BINARY_DIR})

aux_source_directory(. qdbus_message_src)

add_executable(message ${qdbus_message_src})

target_link_librarieS(message Qt5::DBus) 