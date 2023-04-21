# service实现

## 方式一、创建服务并注册对象，建立到session bus的连接

启动程序后，在命令行打开qdbusviewer，查看session bus。
双击Method方法会调用该方法。

代码位于目录service下。

## 方式二、使用Adapter注册Object（推荐方式）

可以直接把test类注册为消息总线上的一个Object，但QT4不推荐。QT4推荐使用Adapter来注册Object。
大多数情况下，可能只需要把自定义的类里的方法有选择的发布到消息总线上，使用Adapter可以很方便的实现选择性发布。
生成Adapter类的流程如下：
使用工具 qdbuscpp2xml从test.h生成XML文件

```
qdbuscpp2xml -M test.h -o com.scorpio.test.xml
```

编辑com.scorpio.test.xml，选择需要发布的method，不需要发布的删除。
使用工具qdbusxml2cpp从XML文件生成继承自QDBusInterface的类

```
qdbusxml2cpp com.scorpio.test.xml -i test.h -a valueAdaptor
```

生成两个文件：valueAdaptor.cpp和valueAdaptor.h

代码位于目录adaptor下，建议先阅读该目录下的README.md。

# 控制台访问service实现

## 方式一、通过QDBusMessage访问Service

## 方式二、通过QDBusInterface 访问Service

## 方式三、从D-Bus XML自动生成Proxy类，调用Proxy类访问Service

代码位于目录proxy下，建议先阅读该目录下的README.md。

/data/opt/Qt5.6.3/5.6.3/gcc_64/lib/cmake/Qt5DBus/Qt5DBusMacros.cmake:87:function(QT5_GENERATE_DBUS_INTERFACE _header) # _customName OPTIONS -some -options )

/data/opt/Qt5.6.3/5.6.3/Src/qtbase/src/dbus/Qt5DBusMacros.cmake:87:function(QT5_GENERATE_DBUS_INTERFACE _header) # _customName OPTIONS -some -options )

/data/opt/Qt5.6.3/5.6.3/Src/qtbase/tests/auto/cmake/test_dbus_module/CMakeLists.txt:22:qt5_generate_dbus_interface(
    
/data/opt/Qt5.6.3/5.6.3/Src/qtdoc/doc/src/development/cmake-manual.qdoc:249:    \row \li qt5_generate_dbus_interface( header [interfacename] OPTIONS ...)
