message(${CMAKE_CURRENT_BINARY_DIR})

qt5_generate_dbus_interface(
    ../qdbus/test.h
    ${CMAKE_CURRENT_BINARY_DIR}/com.scorpio.test.xml
)

qt5_add_dbus_adaptor(
    QDBUS_ADAPTOR_SRCS 
    ${CMAKE_CURRENT_BINARY_DIR}/com.scorpio.test.xml
    test.h
    test
    valueAdaptor
)

include_directories(../qdbus)

aux_source_directory(. QDBUS_ADAPTOR_SRCS)
aux_source_directory(../qdbus QDBUS_ADAPTOR_SRCS)

message(${CMAKE_CURRENT_LIST_LINE})

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "${CMAKE_CURRENT_LIST_LINE} dir='${dir}'")
endforeach()

message(${CMAKE_CURRENT_LIST_LINE})

foreach(_var ${QDBUS_ADAPTOR_SRCS})
	message("${CMAKE_CURRENT_LIST_LINE} 当前变量是：${_var}")
endforeach()

message(${CMAKE_CURRENT_LIST_LINE})

add_executable(adaptor ${QDBUS_ADAPTOR_SRCS})

# target_include_directories(
#     adaptor
#     PRIVATE ../qdbus
# )

target_link_libraries(adaptor Qt5::DBus) 