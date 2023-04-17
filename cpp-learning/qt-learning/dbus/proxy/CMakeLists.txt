message(${CMAKE_CURRENT_BINARY_DIR})

qt5_generate_dbus_interface(
    ../qdbus/test.h
    ${CMAKE_CURRENT_BINARY_DIR}/com.scorpio.test.xml
)

qt5_add_dbus_interface(
    QDBUS_ADAPTOR_SRCS 
    ${CMAKE_CURRENT_BINARY_DIR}/com.scorpio.test.xml
    valueInterface
)

aux_source_directory(. QDBUS_ADAPTOR_SRCS)

add_executable(proxy ${QDBUS_ADAPTOR_SRCS})

target_link_librarieS(proxy Qt5::DBus) 