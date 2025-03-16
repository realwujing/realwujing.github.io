# ip link del veth caused panic

- [Linux 虚拟网络设备 veth-pair 详解，看这一篇就够了](https://www.cnblogs.com/bakari/p/10613710.html)

管理区新架构测试沟通

2022年-公有云-贵州-贵州4.0AZ1-测试 10.246.116.1

os-deploy
  ID    | 主机名                                                                                                 | IP                                       | 备注                                                                 
+-------+--------------------------------------------------------------------------------------------------------+------------------------------------------+-----------------------------------------------------------------------+
  1     | gz15-ct4-az1-os-deploy-10e63e7e11                                                                      | 10.63.7.11                               | OS-other
  2     | gz15-ct4-az1-os-deploy-new-10e63e6e129                                                                 | 10.63.6.129                              | OS-other
  3     | gz15-ct4-yacos-deploy-10e63e7e44                                                                       | 10.63.7.44                               | OS-other

1

10.63.2.11
10.63.2.12
10.63.2.13

ssh secure@10.63.2.13 -p 10000

今天测试 ip link del 直接删除veth，的确会触发panic

这样操作会导致软锁:
```bash
ip link add name veth0 type veth peer name veth1
ovs-vsctl add-port os_manage veth0
ip link del veth0
```

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

![ethtool_vethd4301315](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/ethtool_vethd4301315.png)

```bash
systemctl start openvswitch
ovs-vsctl add-br os_manage
ip link add name veth0 type veth peer name veth1
ip link set veth0 up
ip link set veth1 up
ovs-vsctl add-port os_manage veth0
ovs-vsctl show
ip link set os_manage up
ip addr add 192.168.10.1/24 dev veth0
ip addr add 192.168.10.2/24 dev veth1
ip addr show veth0
ip addr show veth1
ping 192.168.10.1 -I 192.168.10.2
```

```bash
ip link del veth0
ovs-vsctl del-port os_manage veth0
```

![vm_ip_link_del_veth0](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/vm_ip_link_del_veth0.png)

```bash
trace-cmd record -p function_graph -g veth_dellink ip link del veth0
```

```bash
trace-cmd report > vm_ip_link_del_veth0.log
```

```bash
vim vm_ip_link_del_veth0.log

CPU 0 is empty
CPU 1 is empty
CPU 2 is empty
CPU 3 is empty
CPU 4 is empty
CPU 5 is empty
CPU 7 is empty
cpus=8
              ip-2541  [006]  5230.491361: funcgraph_entry:                   |  veth_dellink() {
              ip-2541  [006]  5230.491883: funcgraph_entry:                   |    unregister_netdevice_queue() {
              ip-2541  [006]  5230.491914: funcgraph_entry:                   |      rtnl_is_locked() {
              ip-2541  [006]  5230.491916: funcgraph_entry:      + 56.464 us  |        mutex_is_locked();
              ip-2541  [006]  5230.492126: funcgraph_exit:       ! 212.400 us |      }
              ip-2541  [006]  5230.492173: funcgraph_exit:       ! 291.680 us |    }
              ip-2541  [006]  5230.492182: funcgraph_entry:                   |    unregister_netdevice_queue() {
              ip-2541  [006]  5230.492184: funcgraph_entry:                   |      rtnl_is_locked() {
              ip-2541  [006]  5230.492185: funcgraph_entry:        1.456 us   |        mutex_is_locked();
              ip-2541  [006]  5230.492188: funcgraph_exit:         4.496 us   |      }
              ip-2541  [006]  5230.492190: funcgraph_exit:         7.936 us   |    }
              ip-2541  [006]  5230.492200: funcgraph_exit:       # 1227.888 us |  }

```

```bash
trace-cmd list -f | grep rtnl_dellink
rtnl_dellinkprop
rtnl_dellink
```

```bash
trace-cmd record -p function_graph -g rtnl_dellink ip link del veth0
```

```bash
trace-cmd report > vm_ip_link_del_veth0_rtnl_dellink.log
```

```bash
trace-cmd record -p function_graph ping 192.168.10.1 -I 192.168.10.2
```

```bash
trace-cmd report > vm_ping.log
```

```bash
git push ctkernel-lts-5.10 HEAD:yuanql9/bugfix-pr-veth-peer-soft-lockup -f -u
ba45ce8fe9eb (HEAD -> ctkernel-lts-5.10/yuanql9/bugfix-pr-veth-peer-soft-lockup, ctkernel-lts-5.10/yuanql9/bugfix-pr-veth-peer-soft-lockup) net/core/veth: avoid soft lockup
```

@袁啟良(内核-基础架构部-上海)  我们再13设备上复现了一下，创建完网桥，网桥上的流量越大复现概率越高。
```bash
ovs-vsctl add-br os_manage1
ovs-vsctl add-port os_manage1 bond0.151
ip link add name veth0 type veth peer name veth1
ovs-vsctl add-port os_manage1 veth0
ip link del veth0
ovs-vsctl del-port os_manage1 veth0
```

```bash
bpftrace -e 'kprobe:skb_tx_hash /((struct net_device *)arg0)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)arg0)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:netdev_core_pick_tx /((struct net_device *)arg0)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)arg0)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:__dev_queue_xmit /((struct net_device *)((struct sk_buff *)arg0)->dev)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)((struct sk_buff *)arg0)->dev)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:__dev_queue_xmit /((struct net_device *)((struct sk_buff *)arg0)->dev)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)((struct sk_buff *)arg0)->dev)->num_tc); }'
```
