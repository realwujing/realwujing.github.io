---
date: 2023/04/19 22:00:35
updated: 2023/04/20 14:10:08
---

# flatpak

## demo1

```bash
flatpak install com.belmoussaoui.Decoder 

#  默认过滤规则进入沙箱
flatpak run --devel  --command=bash com.belmoussaoui.Decoder 

# 发送测试命令 需要换成自己的demo服务，然后启动服务，预期结果dbus调用被拦截
dbus-send --session --type=method_call --print-reply --dest=com.deepin.linglong.AppManager /com/deepin/linglong/PackageManager com.deepin.linglong.PackageManager.test string:"org.deepin.demo" 

# 退出沙箱
exit  

# 设置过滤规则允许调用test，进入沙箱
flatpak run  --devel --command=bash --talk-name=com.deepin.linglong.AppManager com.belmoussaoui.Decoder  

# 预期结果dbus调用成功
dbus-send --session --type=method_call --print-reply --dest=com.deepin.linglong.AppManager /com/deepin/linglong/PackageManager com.deepin.linglong.PackageManager.test string:"org.deepin.demo" 
```

## demo2

```bash
flatpak install com.belmoussaoui.Decoder  

# 默认过滤规则进入沙箱  
flatpak run --devel  --command=bash com.belmoussaoui.Decoder  

# 发送测试命令 需要换成自己的demo服务，然后启动服务，预期结果dbus调用被拦截  
dbus-send --session --type=method_call --print-reply --dest=com.scorpio.test /test/objects com.scorpio.test.value.book 

# 退出沙箱

exit

# 设置过滤规则允许调用test，进入沙箱
flatpak run  --devel --command=bash --talk-name=com.scorpio.test com.belmoussaoui.Decoder 

# 预期结果dbus调用成功  

dbus-send --session --type=method_call --print-reply --dest=com.scorpio.test /test/objects com.scorpio.test.value.book 
```

```text
Flatpak原理-ostree 

xdg-dbus-proxy - D-Bus proxy 

Figure 7. Example of successful EXTERNAL authentication with successful negotiation of Unix FD passing 

           C: AUTH EXTERNAL 31303030 

S: OK 1234deadbeef
             

C: NEGOTIATE_UNIX_FD
             

S: AGREE_UNIX_FD
             

C: BEGIN
```

## More

- [https://stackoverflow.com/questions/35813093/creating-a-local-socket-in-a-custom-location-with-qt](https://stackoverflow.com/questions/35813093/creating-a-local-socket-in-a-custom-location-with-qt)
- [https://stackoverflow.com/questions/7753713/c-qt-write-to-unix-socket?rq=1](https://stackoverflow.com/questions/7753713/c-qt-write-to-unix-socket?rq=1)
- [https://stackoverflow.com/questions/52400159/reading-from-a-unix-domain-socket-qlocalsocket-on-qt](https://stackoverflow.com/questions/52400159/reading-from-a-unix-domain-socket-qlocalsocket-on-qt)
- [https://stackoverflow.com/questions/42085259/qt5-is-there-a-way-to-make-qlocalserver-listen-to-abstract-unix-socket](https://stackoverflow.com/questions/42085259/qt5-is-there-a-way-to-make-qlocalserver-listen-to-abstract-unix-socket)
- [https://codereview.qt-project.org/c/qt/qtbase/+/330032](https://codereview.qt-project.org/c/qt/qtbase/+/330032)
- [https://www.cnblogs.com/xiangtingshen/p/11063583.html](https://www.cnblogs.com/xiangtingshen/p/11063583.html)
- [https://dbus.freedesktop.org/doc/dbus-specification.html](https://dbus.freedesktop.org/doc/dbus-specification.html)
