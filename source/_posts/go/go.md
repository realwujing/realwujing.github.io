---
date: 2023/04/21 12:45:22
updated: 2023/04/21 12:45:22
---

# go

## Go 基础

- <https://go.dev/doc/install>

- [Ubuntu go升级](https://juejin.cn/post/6844903952144826375)
- [go语言中文文档](https://www.topgoer.com/)
- [go build命令详解](https://zhuanlan.zhihu.com/p/375530785)

## mod

- [如何使用go module导入本地包](https://www.liwenzhou.com/posts/Go/import_local_package_in_go_module/)
- [Go语言GO111MODULE设置](https://blog.csdn.net/weixin_43874301/article/details/120632571)
- [报错package xxx is not in GOROOT or GOPATH 或者 cannot find package “xxx“ in any of](https://blog.csdn.net/weixin_44676081/article/details/107279746)

## interface

- [想要判断空接口的值，可以使用类型断言](https://www.cnblogs.com/huiyichanmian/p/12800867.html)

## 协程

- [图解Go协程调度原理，小白都能理解 - 恋恋风辰 - 博客园 (cnblogs.com)](https://www.cnblogs.com/secondtonone1/p/11803961.html)
- [<font color=Red>一文读懂什么是进程、线程、协程</font>](https://www.cnblogs.com/Survivalist/p/11527949.html)

## gin

- [<font color=Red>写写代码，喝喝茶，搞搞 Go</font>](https://eddycjy.gitbook.io/golang/di-3-ke-gin/log)
- [POST、DELETE、PUT、GET的含义及区别](https://blog.csdn.net/u014209205/article/details/81205062)
- [Gin实战：Gin+Mysql简单的Restful风格的API](https://www.cnblogs.com/liuzhongchao/p/9244516.html)
- [go gin grpc 参数自动绑定工具.](https://xxjwxc.github.io/post/ginrpc/)
- [Go Gin 简明教程](https://geektutu.com/post/quick-go-gin.html)
- [Gin增删改查demo](https://blog.csdn.net/LitongZero/article/details/109274761)
- [go gin增删改查上传文件下载文件](https://blog.csdn.net/qq_32447301/article/details/108721254)
- [nideshop-admin 基于gin+gorm+vue搭建的nideshop后台管理系统](https://github.com/hairichuhe/nideshop-admin)
- [go项目配置多开发环境 gin框架](https://blog.csdn.net/weixin_43931792/article/details/98765719)

## gorm

- [go语言gorm基本使用](https://blog.csdn.net/yoyogu/article/details/109318626)
- [<font color=Red>GORM 指南</font>](https://gorm.io/zh_CN/docs/conventions.html)
- [关于 gorm AutoMigrate 不自动创建字段的解决方案](https://blog.csdn.net/GsZhuiGui/article/details/107649848)
- [Golang持久化框架 gorm 创建表时 auto_increment 不生效问题](https://blog.csdn.net/duxing_langzi/article/details/115521414)
- [GORM 中文文档_4.4. 数据库迁移](https://www.lmonkey.com/t/XAL231jBR)
- [GORM 2.0 发布说明](https://learnku.com/docs/gorm/v2/v2_release_note/9756)

## redis缓存

- [3.13 优化你的应用结构和实现Redis缓存](https://eddycjy.gitbook.io/golang/di-3-ke-gin/application-redis)
- [一个 Gin 缓存中间件的设计与实现](https://www.cyhone.com/articles/gin-cache/)
- [Gin 集成 go-redis 模块、操作 redis ｜ Go 主题月](https://juejin.cn/post/6945098965457043470)
- [面试官：你可以写一个通用的Redis缓存”装饰器“么？](https://www.51cto.com/article/700690.html)
- [go语言web开发系列之八:gin框架中用go-redis缓存数据](https://blog.csdn.net/weixin_43881017/article/details/111366309)
- [go语言缓存穿透、缓存击穿、缓存雪崩区别和解决方案](https://blog.csdn.net/weixin_42544051/article/details/106562845)
- [redis 间断性耗时长问题解决](https://www.cnblogs.com/xunux/p/5717122.html)
- [面试官：大key和大value的危害，如何处理？](https://www.cnblogs.com/cxy2020/p/13748658.html)
- [redis变慢查询](https://www.cnblogs.com/liliuguang/p/15990542.html)
- [redis 查看key是否存在_Redis为什么变慢了？](https://blog.csdn.net/weixin_34502341/article/details/113580056)
- [Redis变慢原因](https://blog.csdn.net/attack_breast/article/details/113929953)
- [Redis 慢查询详解slowlog](https://blog.csdn.net/qq_26482855/article/details/120511806)

## 上传下载

- [无敌简单快速的文件服务器sgfs](https://www.cnblogs.com/linkstar/p/10429984.html)
- <https://sjqzhang.gitee.io/go-fastdfs/QA.html>

- <https://github.com/sjqzhang/go-fastdfs>

- [Golang原生Web开发入门到微服务-扩展:分块上传](https://www.kancloud.cn/adapa/golang/1116834)
- [golang FastHttp 使用](https://www.cnblogs.com/tomtellyou/p/15155366.html)
- [单文件上传](https://www.kancloud.cn/shuangdeyu/gin_book/949420)
- [golang实现http表单大文件流式上传服务端代码](https://blog.csdn.net/weixin_42681695/article/details/106579169)
- [Golang 超大文件读取的两个方案](https://learnku.com/articles/23559/two-schemes-for-reading-golang-super-large-files)
- [golang写服务端程序，作为文件上传与下载的服务器。配合HTML5以网页作为用户页面](https://studygolang.com/articles/7389)
- [使用multipart/form-data实现文件的上传与下载](https://tonybai.com/2021/01/16/upload-and-download-file-using-multipart-form-over-http/)
- [服务器上传下载问题之分块上传（断点续传）](https://chunlife.top/2019/04/09/%E6%9C%8D%E5%8A%A1%E5%99%A8%E4%B8%8A%E4%BC%A0%E4%B8%8B%E8%BD%BD%E9%97%AE%E9%A2%98%E4%B9%8B%E5%88%86%E5%9D%97%E4%B8%8A%E4%BC%A0%EF%BC%88%E6%96%AD%E7%82%B9%E7%BB%AD%E4%BC%A0%EF%BC%89/)
- [3、gin 静态文件服务](https://blog.csdn.net/u011327801/article/details/101365018)
- [一文搞懂如何利用multipart/form-data实现文件的上传与下载](https://blog.csdn.net/bigwhite20xx/article/details/112792041)
- [本文介绍对象存储OSS的Go SDK各种使用场景下的示例代码。](https://help.aliyun.com/document_detail/32144.htm?spm=a2c4g.11186623.0.0.401f24cbVPHe9f#concept-32144-zh)

## 日志

- [[系列] Gin框架 - 使用 Logrus 进行日志记录](https://juejin.cn/post/6844903896922456071)
- [Gin添加基于logrus的日志组件](https://codeantenna.com/a/Ux6JIA4WSo)
- [6 将日志保存到文件](http://www.lsdcloud.com/go/middleware/logrus.html#_6-%E5%B0%86%E6%97%A5%E5%BF%97%E4%BF%9D%E5%AD%98%E5%88%B0%E6%96%87%E4%BB%B6)
- [37go语言学习之日志配置logrus](https://blog.csdn.net/xiaobo5264063/article/details/120711908)
- [logrus自定义日志输出格式](https://cloud.tencent.com/developer/article/1830707)
- <https://github.com/uber-go/zap>

## 序列化

- [Go的json解析：Marshal与Unmarshal](https://blog.csdn.net/zxy_666/article/details/80173288)

- 代码生成

- [<font color=Red>https://mholt.github.io/json-to-go/</font>](https://mholt.github.io/json-to-go/)

- [<font color=Red>https://mholt.github.io/curl-to-go/</font>](https://mholt.github.io/curl-to-go/)

## shell

- [Go语言调用Shell与可执行文件](https://www.jianshu.com/p/dd8a113b02a3)
- [golang 执行linux命令 &获取命令执行返回码，命令pid，执行结果(逐行输出)](https://blog.csdn.net/YMY_mine/article/details/101068865)
- [golang exec.Command 执行命令用法实例](https://blog.csdn.net/whatday/article/details/109277998)
- [GO定时任务CRON执行不成功？看一下这篇文章就明白了](https://www.helloworld.net/p/nYp4Sg1iwvI4J)

## debug

- [VS Code 断点调试golang](https://segmentfault.com/a/1190000018671207)
- [golang-进程崩溃后如何输出错误日志？core dump](https://blog.csdn.net/xmcy001122/article/details/105665732)

## swag

- [Golang Gin 实践-连载八-为它加上Swagger.md](https://www.bookstack.cn/read/gin-EDDYCJY-blog/golang-gin-2018-03-18-Gin%E5%AE%9E%E8%B7%B5-%E8%BF%9E%E8%BD%BD%E5%85%AB-%E4%B8%BA%E5%AE%83%E5%8A%A0%E4%B8%8ASwagger.md)
- [go swag常用注释](https://blog.csdn.net/qq_42873554/article/details/118797414)
- [使用swaggo自动生成Restful API文档](https://ieevee.com/tech/2018/04/19/go-swag.html)
- [<font color=Red>swag-api - pkg.dev</font>](https://pkg.go.dev/github.com/swaggo/swag/example/basic/api#Upload)
- [Swaggo](https://www.topgoer.com/%E5%85%B6%E4%BB%96/Swagger.html)
- [gin-swagger的安装使用（注释参数说明）](https://blog.csdn.net/qq_45100706/article/details/115481714)

## 文件

- [go：获取文件的名称、前缀、后缀](https://zhuanlan.zhihu.com/p/80403583)

## elf

- [pkg debug/elf 应用](https://www.hitzhangjie.pro/debugger101.io/7-headto-sym-debugger/6-gopkg-debug/1-elf.html)
- [type Header64（查看源代码）](http://www.verydoc.net/go/00003845.html)
- <https://go.dev/src/debug/elf/file.go>

- [后端 第五日：readelf 开发过程之疑难排解](https://www.dazhuanlan.com/xtghdnui/topics/1847414)

## 压力测试

- [QPS、TPS、PV、UV、GMV、IP、RPS含义简单说明](https://blog.csdn.net/lirui8412973/article/details/100582177)
- [QPS、TPS、PV、UV、GMV、IP、RPS 是什么鬼](https://www.cnblogs.com/lyc88/articles/11319559.html)
- [服务端压测指标评估](https://www.cnblogs.com/yangxiayi1987/p/14840191.html)
- [性能测试工具 wrk,ab,locust,Jmeter 压测结果比较](https://testerhome.com/topics/17068)
- [go实现的压测工具【单台机器100w连接压测实战】](https://cloud.tencent.com/developer/article/1509809)
- [优化nginx-ingress-controller并发性能](https://cloud.tencent.com/developer/article/1537695)

## 性能优化

- [Golang pprof 性能分析与火焰图](https://blog.csdn.net/weixin_37717557/article/details/108684433)
- [Gin框架中使用pprof](http://liumurong.org/2019/12/gin_pprof/)
- [Golang 大杀器之性能剖析 PProf](https://segmentfault.com/a/1190000016412013)
- <https://github.com/gin-contrib/pprof>

## 编译

- [Golang交叉编译各平台的可执行二进制程序](https://lanshiqin.com/92119e60/)
- [go build 命令参数详解](https://blog.csdn.net/cyq6239075/article/details/103911098)

## 安全

- [随机密码生成器](https://www.roboform.com/cn/password-generator)
- [10 种保证接口数据安全的方案！](https://mp.weixin.qq.com/s/NVPuFAjt34_jhFqW2tG7MQ)
- [全网最全的权限系统设计方案（图解）](https://mp.weixin.qq.com/s/oRTdMjqZf59cjAqR72g4aA)

- [详解如何在Go语言中调用C源代码](https://www.jb51.net/article/247194.htm)

## 其他

- <https://githubmemory.com/repo/justjanne/powerline-go/issues/272>
- <https://golangrepo.com/repo/utkusen-urlhunter-go-web-applications>

- [Go项目开源规范](https://www.shuzhiduo.com/A/6pdDN0eXJw/)

- [新来的 CTO 规定所有接口都用 post 请求...](https://mp.weixin.qq.com/s/eU9lMmomNHDrLZw4eV31Xw)
