---
date: 2023/04/21 11:43:50
updated: 2024/03/14 10:59:01
---

# python

## anaconda

- [Anaconda python3.6版本安装_华仔的博客-CSDN博客_anaconda python3.6](https://blog.csdn.net/qq_36338754/article/details/96430294)
- `wget https://mirrors.tuna.tsinghua.edu.cn/anaconda/archive/Anaconda3-5.2.0-Linux-x86_64.sh`

- [pipreqs查找python项目依赖并生成requirements.txt - 一抹浅笑 - 博客园 (cnblogs.com)](https://www.cnblogs.com/zhaopanpan/p/9383350.html)
- [python使用清华源进行pip安装的方法（最方便，不用换源只需一行代码）](https://zhuanlan.zhihu.com/p/129866307)
- [【2020-06】3个Anaconda国内开源镜像站](https://blog.csdn.net/weixin_43667077/article/details/106521015)
- [Python使用清华大学镜像源](https://blog.csdn.net/liuYinXinAll/article/details/90042947)
- [pipreqs（找当前项目依赖的包）](https://www.cnblogs.com/believepd/p/10423094.html)
- [python国内镜像源](https://www.cnblogs.com/songzhixue/p/11296720.html)
- [<font color=Red>Ubuntu中conda的安装及常用命令</font>](https://blog.csdn.net/weixin_40922744/article/details/109866687)
- [anaconda | 镜像站使用帮助 | 清华大学开源软件镜像站 | Tsinghua Open Source Mirror](https://mirrors.tuna.tsinghua.edu.cn/help/anaconda/)

    ```bash
    pip config set global.index-url https://pypi.mirrors.ustc.edu.cn/simple/
    ```

### Anaconda3-2022.05-Linux-x86_64.sh

```bash
wget https://mirrors.tuna.tsinghua.edu.cn/anaconda/archive/Anaconda3-2022.05-Linux-x86_64.sh
sudo mkdir -p /opt/anaconda3
sudo chmod +x Anaconda3-2022.05-Linux-x86_64.sh
sudo chmod -R 777 /opt/anaconda3
./Anaconda3-2022.05-Linux-x86_64.sh -u
```

在启动终端时取消自动激活 base 环境:

```bash
conda config --set auto_activate_base false
```

```bash
conda activate base
```

```bash
conda deactivate
```

将社区驱动的conda-forge channel添加到你的channel列表中：

```bash
conda config --append channels conda-forge
```

## 日志

- [python 日志 logging模块(详细解析)_pansaky的博客-CSDN博客](https://blog.csdn.net/pansaky/article/details/90710751)
- [loguru · PyPI](https://pypi.org/project/loguru/)
- [Python之日志处理（logging模块） - 云游道士 - 博客园 (cnblogs.com)](https://www.cnblogs.com/yyds/p/6901864.html)
- [python logging模块打印log到指定文件_Lucky小黄人的博客-CSDN博客](https://blog.csdn.net/qq_41767116/article/details/113734410)
- [python logging 日志 通过修饰器获取错误信息_桜-CSDN博客](https://blog.csdn.net/qq_38641985/article/details/81672283)
- [python日志logging模块(详细解析)](https://blog.csdn.net/pansaky/article/details/90710751)
- [Python + logging 输出到屏幕，将log日志写入文件_lvmengzou的专栏-CSDN博客](https://blog.csdn.net/lvmengzou/article/details/118307249)
- [loguru如何创建两个不同的日志文件_Jason_WangYing的博客-CSDN博客](https://blog.csdn.net/Jason_WangYing/article/details/114155112)

## 爬虫

- [scrapy框架中多个spider,tiems,pipelines的使用及运行方法](https://www.cnblogs.com/nmsghgnv/p/12369656.html)
- [Scrapy：配置日志](https://www.cnblogs.com/jackzz/p/10774517.html)
- [python 爬虫面试整理](https://blog.csdn.net/weixin_43958804/article/details/88308992)
- <https://github.com/realReid/LHB.git>

- [xpath 如何选取某个节点但是排除掉某个子节点？ - 知乎 (zhihu.com)](https://www.zhihu.com/question/56148271)
- [Python爬虫实战(一) 用Python爬取百度百科 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/78571606)
- [Xpath 详解 - 简书 (jianshu.com)](https://www.jianshu.com/p/6a0dbb4e246a)
- [Python - urllib - 听雨危楼 - 博客园 (cnblogs.com)](https://www.cnblogs.com/Neeo/articles/11520952.html)
- [[Python]requests使用代理 - 简书 (jianshu.com)](https://www.jianshu.com/p/c8f896d668d6)

## python mysql

- [python3 实现mysql数据库连接池 - jiangxiaobo - 博客园 (cnblogs.com)](https://www.cnblogs.com/jiangxiaobo/p/12786205.html)
- [pymysql 线程安全pymysqlpool - 公众号python学习开发 - 博客园 (cnblogs.com)](https://www.cnblogs.com/c-x-a/p/9045646.html)
- [Python MySQL数据库连接池组件pymysqlpool详解_Python_脚本之家 (zzvips.com)](http://www.zzvips.com/article/117347.html)
- [Python MySQL数据库连接池组件pymysqlpool详解 - 码农教程 (manongjc.com)](http://www.manongjc.com/detail/5-mnpaewuzaaewzpm.html)
- [MySQL插入数据时自动添加创建时间（create_time）和修改时间（update_time）_wait_2030的博客-CSDN博客](https://blog.csdn.net/wait_2030/article/details/90266211)
- [Python 使用 PyMysql、DBUtils 创建连接池，提升性能 - feiquan - 博客园 (cnblogs.com)](https://www.cnblogs.com/feiquan/p/11350374.html)

## pandas

- [python如何转换dataframe列的类型astype()方法（超级详细）_爱代码的小哥的博客-CSDN博客](https://blog.csdn.net/weixin_51098806/article/details/115265280)
- [pandas从mysql读写数据 - 简书 (jianshu.com)](https://www.jianshu.com/p/3d797335f467)
- [pandas 数据类型转换 - 多一点 - 博客园 (cnblogs.com)](https://www.cnblogs.com/onemorepoint/p/9404753.html)
- [在pandas中遍历DataFrame行_ls13552912394的博客-CSDN博客_dataframe遍历行](https://blog.csdn.net/ls13552912394/article/details/79349809)
- [pandas df 遍历行方法 - wqbin - 博客园 (cnblogs.com)](https://www.cnblogs.com/wqbin/p/11775812.html)
- [python基于Pandas读写MySQL数据库_python_脚本之家 (jb51.net)](https://www.jb51.net/article/209858.htm)
- [Pandas 中 SettingwithCopyWarning 的原理和解决方案 - 简书 (jianshu.com)](https://www.jianshu.com/p/72274ccb647a)
- [python pandas >loc、iloc用法 - 简书 (jianshu.com)](https://www.jianshu.com/p/732858f89a00)
- [(3条消息) Pandas - A value is trying to be set on a copy of a slice from a DataFrame_NFII-CSDN博客](https://blog.csdn.net/qq_42711381/article/details/90451301)
- [(3条消息) 处理pandas出现warning: “A value is trying to be set on a copy of a slice from a DataFrame.”_curry3030的博客-CSDN博客](https://blog.csdn.net/curry3030/article/details/100533296)
- [python pandas Dataframe增加一列遇到A value is trying to be set on a copy of a slice from a DataFrame._yudajiangshan的博客-CSDN博客](https://blog.csdn.net/yudajiangshan/article/details/112402130)
- [收藏学习：100个Pandas常用的函数](https://mp.weixin.qq.com/s/BNMj5VL3QW6Qjg6WoKqCXA)

## 机器学习

- [Python技术交流与分享](http://www.feiguyunai.com/)
- <https://github.com/Kunal-Varma/GPT3-Demos>

- [莫烦python](https://mofanpy.com/)
- [NLP民工的乐园:几乎最全的中文NLP资源库](https://github.com/fighting41love/funNLP)
- [ApacheCN深度学习译文集](https://github.com/apachecn/apachecn-dl-zh)
- [PyTorch中文官方教程1.7](https://pytorch.apachecn.org/docs/1.7/)
- [Python+gensim-文本相似度分析（小白进）](https://blog.csdn.net/Yellow_python/article/details/81021142)
- [机器学习&数据挖掘笔记_16（常见面试之机器学习算法思想简单梳理）](https://www.cnblogs.com/tornadomeet/p/3395593.html)
- [机器学习笔试面试超详细总结（一）](https://blog.csdn.net/jiaoyangwm/article/details/79805939)

## 其他

- [Python实例方法、静态方法和类方法详解（包含区别和用法） (biancheng.net)](http://c.biancheng.net/view/4552.html)
- [110道python面试题](https://www.cnblogs.com/finer/p/12846475.html)
- [吐血总结！50道Python面试题集锦（附答案）](https://blog.csdn.net/sinat_38682860/article/details/94763641)
- [【进阶Python】第六讲：单例模式的妙用 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/87524388)
- [函数式编程 - 廖雪峰的官方网站 (liaoxuefeng.com)](https://www.liaoxuefeng.com/wiki/1016959663602400/1017328525009056)
- [ubuntu 上的python安装包位置](https://blog.51cto.com/u_15127500/3822046)
