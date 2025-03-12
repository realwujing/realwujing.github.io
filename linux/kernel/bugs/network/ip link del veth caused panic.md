# ip link del veth caused panic

- [Linux 虚拟网络设备 veth-pair 详解，看这一篇就够了](https://www.cnblogs.com/bakari/p/10613710.html)

管理区新架构测试沟通

2022年-公有云-贵州-贵州4.0AZ1-测试 10.246.116.1

10.246 117.197
10.246.117.191

今天测试 ip link del 直接删除veth，的确会触发panic

![openvswitch_dmesg](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/openvswitch_dmesg.png)

```bash
sysctl -w kernel.hung_task_panic=1
sysctl -w kernel.softlockup_panic=1
```

![netdev_pick_tx+0x27c0x294](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/netdev_pick_tx+0x27c0x294.png)

![net_core_dev_3910](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/net_core_dev_3910.png)

![vethd4301315_messages](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/vethd4301315_messages.png)

![ovs_vswitchd](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/ovs_vswitchd.png)

![interface_vethd4301315](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/interface_vethd4301315.png)

下方这两个测试用例说明虚拟网口存在很严重的bug:

![ip_link_del_veth2390bec0](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/ip_link_del_veth2390bec0.png)

![ifconfig_vethd5e59fb7_down](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/ifconfig_vethd5e59fb7_down.png)