# CMake

```bash
make -j CFLAGS="-O0 -g"
```

```bash
make -j12 VERBOSE=1 CXX_FLAGS="$CXX_FLAGS -g -O3 -fPIC"
```

- <https://cmake.org/cmake/help/v3.17/guide/tutorial/index.html#adding-system-introspection-step-5>

- <https://docs.w3cub.com/cmake~3.19/command/target_link_directories>

- [cmake 常用指令入门指南](https://www.cnblogs.com/yinheyi/p/14968494.html)
- [<font color=Red>超详细的CMake教程</font>](https://www.cnblogs.com/ybqjymy/p/13409050.html)
- [<font color=Red>CMake 教程 | CMake 从入门到应用</font>](https://aiden-dong.github.io/2019/07/20/CMake%E6%95%99%E7%A8%8B%E4%B9%8BCMake%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%BA%94%E7%94%A8/)
- [<font color=Red>text</font>CMake简明教程(ubuntu)](https://www.cnblogs.com/spmt/p/12632322.html)
- [<font color=Red>【使用CMake组织C++工程】2：CMake 常用命令和变量</font>](https://elloop.github.io/tools/2016-04-10/learning-cmake-2-commands)
- [<font color=Red>CMake菜谱（CMake Cookbook中文版）</font>](http://www.mianshigee.com/tutorial/CMake-Cookbook/content-chapter1-1.1-chinese.md)
- [<font color=Red>CMake菜谱（CMake Cookbook中文版）</font>](https://www.bookstack.cn/read/CMake-Cookbook/README.md)
- [使用CMake构建C++项目 ](https://zhuanlan.zhihu.com/p/92928820)

    ```bash
    make -j VERBOSE=1
    ```

- [CMake 手册详解](https://www.cnblogs.com/lsgxeva/p/9454443.html)
- [【学习CMake】 - 如何高效使用"cmake help" ?](https://blog.csdn.net/KYJL888/article/details/100938384)
- [CMakeLists.txt语法介绍与实例演练自定义编译选项](https://blog.csdn.net/afei__/article/details/81201039)
- [CMake学习笔记（二）——CMake语法](https://blog.csdn.net/ajianyingxiaoqinghan/article/details/70230902)
- [CMakeList模板(二)：编译多个工程](https://blog.csdn.net/lianshaohua/article/details/107783811)
- [CMakeLists.txt编写常用命令 - 星星,风,阳光 - 博客园 (cnblogs.com)](https://www.cnblogs.com/xl2432/p/11225276.html)
- [set追加变量的值](https://www.cnblogs.com/xl2432/p/11225276.html#1-set%E7%9B%B4%E6%8E%A5%E8%AE%BE%E7%BD%AE%E5%8F%98%E9%87%8F%E7%9A%84%E5%80%BC)

- [cmake / cmake build 如何理解](https://blog.csdn.net/itworld123/article/details/123862402)

## find_package

- [<font color=Red>“轻松搞定CMake”系列之find_package用法详解</font>](https://blog.csdn.net/zhanghm1995/article/details/105466372)
- [<font color=Red>深入理解CMake(3):find_package()的使用</font>](https://www.jianshu.com/p/39fc5e548310)
- [[CMake] find_package 指定路径](https://blog.csdn.net/weixin_43742643/article/details/113858915)
- [cmake中find_package的查找路径](https://www.jianshu.com/p/243ff97bbbc6)
- [<font color=Red>深入理解CMake(5)：find_package寻找手动编译安装的Protobuf过程分析</font>](https://www.jianshu.com/p/5dc0b1bc5b62)
- [CMake：如果你需要指定CMAKE_MODULE_PATH，find_package（）有什么用？](https://cloud.tencent.com/developer/ask/87956)

## pkg_check_modules pkg_search_module

- [CMAKE查找库：find_package和pkg_check_moduls的区别](https://blog.csdn.net/feccc/article/details/107160668)

## pkg-config

- [cmake 使用pkg-config配置第三方库和头文件](https://blog.csdn.net/zxcasd11/article/details/104010621)
- [cmake：pkg_check_modules](https://blog.csdn.net/zhizhengguan/article/details/111826697)
- [使用 pkg-config 让 C++ 工程编译配置更灵活](https://zhuanlan.zhihu.com/p/417285806)

## install

- [<font color=Red>【CMake】cmake的install指令</font>](https://blog.csdn.net/qq_38410730/article/details/102837401)
- [CMake之install方法的使用](https://zhuanlan.zhihu.com/p/102955723)

## uninstall

- [卸载 make install 编译安装的软件](https://blog.csdn.net/reasonyuanrobot/article/details/106732047)
- [linux里用cmake安装的软件要怎么卸载？](https://www.zhihu.com/question/21203756)

- [cmake Debug模式和Release模式](https://blog.csdn.net/liujiayu2/article/details/50219377)
- [自动化构建 - cmake - 构建目标类型 - Debug，Release，RelWithDebInfo，MinSizeRel](https://blog.csdn.net/qazw9600/article/details/115267688)
- [CMake向解决方案添加源文件兼头文件](https://www.cxyzjd.com/article/weixin_30706507/96058094)
- [cmake中in/out-source编译](http://blog.sina.com.cn/s/blog_ad0672d60102zaho.html)

## 环境变量

- [七、cmake 常用变量和常用环境变量](https://www.kancloud.cn/itfanr/cmake-practice/82989)
- [cmake:在各级子项目(目录)之间共享变量](https://blog.csdn.net/10km/article/details/50508184)
- [查看CMake变量的默认值方法](https://blog.csdn.net/shawzg/article/details/108593010)
- [cmake(十七)Cmake的foreach循环和while循环](https://blog.csdn.net/wzj_110/article/details/116110014)
- [CMake之CMakeLists.txt编写入门](https://blog.csdn.net/z_h_s/article/details/50699905)

- [<font color=Red>cmake-打印Include路径列表</font>](http://qianchenglong.github.io/2015/01/29/cmake-%E6%89%93%E5%8D%B0Include%E8%B7%AF%E5%BE%84%E5%88%97%E8%A1%A8/)

## 可见性

- [PRIVATE INTERFACE PUBLIC](https://www.bookstack.cn/read/CMake-Cookbook/content-chapter1-1.8-chinese.md)
- <https://leimao.github.io/blog/CMake-Public-Private-Interface/>

- [cmake 之 PUBLIC|PRIVATE|INTERFACE 关键字](https://ravenxrz.ink/archives/e40194d1.html)
- [<font color=Red>cmake：target_** 中的 PUBLIC，PRIVATE，INTERFACE</font>](https://zhuanlan.zhihu.com/p/82244559)
- [target_link_libraries命令 PRIVATE|PUBLIC|INTERFACE的作用](https://its201.com/article/znsoft/119035578)
- [CMake可以使用Graphviz图形可视化软件(http://www.graphviz.org )生成项目的依赖关系图:](https://www.bookstack.cn/read/CMake-Cookbook/content-chapter7-7.7-chinese.md)
- [cmake：使用execute_process调用shell命令或脚本](https://blog.csdn.net/qq_28584889/article/details/97758450)

## 测试

- [C++语言的单元测试与代码覆盖率](https://paul.pub/gtest-and-coverage/)
- [Gtest集成Lcov代码覆盖率测试](https://www.codeleading.com/article/93614362313/)
- [使用 Google Test 测试框架](https://www.jianshu.com/p/2d3c2c44449a)
- [GoogleTest测试框架介绍（一）](https://liitdar.blog.csdn.net/article/details/85712973)
- [Ubuntu Google test 单元测试](https://blog.csdn.net/boy854456187/article/details/117165221)

## cpack

- [cmake(12)：使用cpack生成DEB二进制文件](https://blog.csdn.net/rangfei/article/details/122817575)
- [将工程使用CPack工具打包成为一个deb包](https://www.cnblogs.com/mxnote/articles/16816354.html)
