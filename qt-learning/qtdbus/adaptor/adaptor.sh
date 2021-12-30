# 使用工具 qdbuscpp2xml从test.h生成XML文件
qdbuscpp2xml -M ../qdbus/test.h -o com.scorpio.test.xml
# 编辑com.scorpio.test.xml，选择需要发布的method，不需要发布的删除。

## 使用工具qdbusxml2cpp从XML文件生成继承自QDBusInterface的类
qdbusxml2cpp com.scorpio.test.xml -i ../qdbus/test.h -a valueAdaptor
# 生成两个文件：valueAdaptor.cpp和valueAdaptor.h
