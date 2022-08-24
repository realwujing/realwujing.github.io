# ebpf-learning

- [BPF Compiler Collection (BCC)](BCC.md)
- [bpftrace](bpftrace.md)

## 插桩

- 静态插桩：在已有目标App源码的前提下，加入实现的framework代码一起编译。
- 动态插桩：在没有目标App源码的前提下，通过一些技术手段实现对目标app的ipa安装包的修改，再将修改后的app安装到手机设备上，从而改变目标app的表现行为。
- [插桩特性](https://qt4i.readthedocs.io/zh_CN/latest/advance/stub.html)