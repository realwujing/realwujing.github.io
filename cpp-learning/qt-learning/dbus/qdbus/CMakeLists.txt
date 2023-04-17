# message(${CMAKE_CURRENT_BINARY_DIR})

# 使用工具qdbuscpp2xml从object.h生成XML文件；
# qdbuscpp2xml -M test.h -o com.scorpio.test.xml
qt5_generate_dbus_interface(test.h com_scorpio_test.xml)

# 解析com.scorpio.test.xml文件，生成Adaptor类ValueAdaptor，文件名称为valueAdaptor.h、valueAdaptor.cpp命令行如下：
# qdbusxml2cpp com.scorpio.test.xml -i test.h -a valueAdaptor
qt5_add_dbus_adaptor(QDBUS_SRCS com.scorpio.test.xml test.h test valueAdaptor)

# 解析com.scorpio.test.xml文件，生成Proxy类ComScorpioTestValueInterface，文件名称为testInterface.h、testInterface.cpp命令行如下：
# qdbusxml2cpp com.scorpio.test.xml -p testInterface
qt5_add_dbus_interface(${CMAKE_CURRENT_BINARY_DIR} com.scorpio.test.xml testInterface)

# qt5_add_dbus_interface(QDBUS_SRCS com.scorpio.test.xml valueInterface)

foreach(_var ${QDBUS_SRCS})
	message("当前变量是：${_var}")
endforeach()

# message(${PROJECT_BINARY_DIR})
