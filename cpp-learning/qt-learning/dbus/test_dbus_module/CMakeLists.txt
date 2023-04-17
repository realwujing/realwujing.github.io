set(CMAKE_INCLUDE_CURRENT_DIR ON)

aux_source_directory(. my_srcs)

qt5_generate_dbus_interface(
    mydbusobject.h
    ${CMAKE_CURRENT_BINARY_DIR}/org.qtProject.Tests.MyDBusObject.xml
)

qt5_add_dbus_adaptor(my_srcs
    ${CMAKE_CURRENT_BINARY_DIR}/org.qtProject.Tests.MyDBusObject.xml
    mydbusobject.h
    MyDBusObject
)

add_executable(myobject ${my_srcs} )
target_link_libraries(myobject Qt5::DBus)
