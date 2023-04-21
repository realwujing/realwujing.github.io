# 使用工具qdbuscpp2xml从object.h生成XML文件
qdbuscpp2xml -M ../qdbus/test.h -o com.scorpio.test.xml

# 使用工具qdbusxml2cpp从XML文件生成继承自QDBusInterface的类#
qdbusxml2cpp com.scorpio.test.xml -p valueInterface
# 生成两个文件：valueInterface.cpp和valueInterface.h

## 运行proxy.sh执行上述命令
```
./proxy.sh
```
