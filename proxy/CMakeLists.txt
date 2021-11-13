message(${CMAKE_CURRENT_BINARY_DIR})

include_directories(.)

aux_source_directory(. qdbus_proxy_src)

add_executable(proxy ${qdbus_proxy_src})

target_link_librarieS(proxy Qt5::DBus) 