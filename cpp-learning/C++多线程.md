# 多线程

## 基础

- [高并发高性能服务器是如何实现的](https://mp.weixin.qq.com/s/Z07Hc9SRfGz6n8XhFHGVyA)
- [看完这篇还不懂高并发中的线程与线程池你来打我(内含20张图)](https://blog.csdn.net/github_37382319/article/details/108478589)
- [Linux 多线程全面解析](https://mp.weixin.qq.com/s/2eetiUAtedavND8c-sQM5w)
- [带你走进Linux内核源码中最常见的数据结构之「mutex」](https://www.vxbus.com/post/linux/linux-kernel-source-code-data-structure-mutex.html)

### pthread
- [写给大忙人看的进程和线程](https://mp.weixin.qq.com/s/CCzEFnyxXKp6a1iVSBD78w)
- [详解多线程](https://www.cnblogs.com/yjboke/p/8911220.html)
- [<font color=Red>【Linux】多线程详解，一篇文章彻底搞懂多线程中各个难点！！！</font>](https://blog.csdn.net/w903414/article/details/110005612)

- [<font color=Red>【Linux】多线程同步的四种方式 - Y先森0.0 - 博客园 (cnblogs.com)</font>](https://www.cnblogs.com/yinbiao/p/11190336.html)
- [<font color=Red>信号量函数（semget、semop、semctl）及其范例</font>](https://blog.csdn.net/guoping16/article/details/6584043)
- [「linux」深入理解RCU核心原理](https://www.toutiao.com/article/6978007709220291111)
- [<font color=Red>linux c/c++开发：多线程并发锁：互斥锁、自旋锁、原子操作、CAS</font>](https://www.toutiao.com/article/7106767459059089923)

- [详解Linux多线程中互斥锁、读写锁、自旋锁、条件变量、信号量](https://mp.weixin.qq.com/s/QITWTjR1T9eVBwPJ6sGZBA)

- [<font color=Red>pthread_cond_wait()用法分析</font>](https://blog.csdn.net/hairetz/article/details/4535920)
- [<font color=Red>linux对线程等待和唤醒操作（pthread_cond_timedwait 详解）</font>](https://blog.csdn.net/wteruiycbqqvwt/article/details/99707580)

#### 信号量
- [深入讲解读写信号量（上）](https://www.toutiao.com/article/7159838260959740457/)
- [深入讲解读写信号量（下）](https://www.toutiao.com/article/7159867610317390372)

- [【C++多线程】join()及注意 - Chen沉尘 - 博客园 (cnblogs.com)](https://www.cnblogs.com/chen-cs/p/13055211.html)

### C++11多线程
- [从无栈协程到 C++异步框架](https://www.toutiao.com/article/7153230059011686948/)
- [c++11中关于std::thread的join函数详解_fanyun的博客-CSDN博客](https://blog.csdn.net/fanyun_01/article/details/100178104)

- [C++11 并发指南系列 - Haippy - 博客园 (cnblogs.com)](https://www.cnblogs.com/haippy/p/3284540.html)
- [C++11 并发指南五(std::condition_variable 详解) - Haippy - 博客园 (cnblogs.com)](https://www.cnblogs.com/haippy/p/3252041.html)
- [C++11条件变量：notify_one()与notify_all()的区别_feikudai8460的博客-CSDN博客\_c++ notify\_one](https://blog.csdn.net/feikudai8460/article/details/109604690)

### 死锁

- [手把手带你实现一个死锁检测组件](https://www.toutiao.com/article/7131231200097862158)
- [2.4.2 死锁的处理策略-预防死锁（破坏互斥条件、破坏不可剥夺条件、破坏请求和保持条件、破坏循环等待条件）](https://blog.csdn.net/qq_26553393/article/details/122281044)
- [<font color=Red>Linux下排除死锁详细教程（基于C++11、GDB）</font>](https://blog.csdn.net/zsiming/article/details/126695393)
- [【开发工具】【lockdep】Linux内核死锁检测工具（lockdep）的使用](https://blog.csdn.net/Ivan804638781/article/details/100740857)
- [手画图解 | 关于死锁，面试的一切都在这里了](https://mp.weixin.qq.com/s/Jjio-cNapfUMBqEOefi1og)

## 线程池

- [C++实现线程池_蓬莱道人的博客-CSDN博客_c++实现线程池](https://blog.csdn.net/MOU_IT/article/details/88712090)
- [C++11的简单线程池代码阅读-乌合之众-博客园(cnblogs.com)](https://www.cnblogs.com/oloroso/p/5881863.html)
- [<font color=Red>C++ 线程池</font>](https://wangpengcheng.github.io/2019/05/17/cplusplus_theadpool/)
- [c++11：线程池，boost threadpool、thread_group example_zzhongcy的专栏-CSDN博客_boost 线程池](https://blog.csdn.net/zzhongcy/article/details/89453370)
- [线程池原理及创建（C++实现）- DoubleLi -博客园(cnblogs.com)](https://www.cnblogs.com/lidabo/p/3328402.html)
- [C++线程池原理及创建（转）- cpper-kaixuan -博客园(cnblogs.com)](https://www.cnblogs.com/cpper-kaixuan/articles/3640485.html)
- [深入解析C++编程中线程池的使用_C 语言_脚本之家 (jb51.net)深入解析C++编程中线程池的使用_C 语言_脚本之家 (jb51.net)](https://www.jb51.net/article/75295.htm)
- [线程池原理及C语言实现线程池](https://blog.csdn.net/qq_36359022/article/details/78796784)
- [c++/c实现线程池](https://blog.csdn.net/robothj/article/details/80172287)
- [threadpool/Main.cpp at master · lzpong/threadpool (github.com)](https://github.com/lzpong/threadpool/blob/master/Main.cpp)
- [基于C++11的线程池(threadpool),简洁且可以带任意多的参数 - _Ong - 博客园 (cnblogs.com)](https://www.cnblogs.com/lzpong/p/6397997.html)

## 数据库连接池

- [C++数据库连接池的设计与实现_暮明已逝的博客-CSDN博客_c++数据库连接池](https://blog.csdn.net/weixin_43825537/article/details/104516274)
- [基于c++的数据库连接池的实现与理解(toutiao.com)](https://www.toutiao.com/i7004734534830801420/)

## 其他

- [c++ - 执行并行任务而无需等待C++中的结果](https://www.coder.work/article/1954428)
- <https://github.com/seanmiddleditch/jobxx>

- <https://github.com/cdwfs/cds_job>

- <https://github.com/delscorcho/basic-job-system>
