---
title: 'ubuntu22.04 panic on insmod'
date: '2025/11/21 18:06:27'
updated: '2025/11/21 18:06:27'
---

# ubuntu22.04 panic on insmod

![00232bfa8c17cc5](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/00232bfa8c17cc5.png)

```bash
[54278.187695] Unable to handle kernel paging request at virtual address 000232bfa8c17cc5
[54278.187703] Mem abort info:
[54278.187706]   ESR = 0x00000004
[54278.187709]   EC = 0x25: DABT (current EL), IL = 32 bits
[54278.187712]   SET = 0, FnV = 0
[54278.187714]   EA = 0, S1PTW = 0
[54278.187717]   FSC = 0x04: level 0 translation fault
[54278.187720] Data abort info:
[54278.187722]   ISV = 0, ISS = 0x00000004
[54278.187725]   CM = 0, WnR = 0
[54278.202473] [00232bfa8c17cc5] address between user and kernel address ranges
[54278.219341] Internal error: Oops: 96000004 [#1] SMP
[54278.219353] Modules linked in: tramme1(OE) xt_conntrack nft_chain_nat xt_MASQUERADE nf_nat nf_conntrack_netlink nf_conntrack nf_defrag_ipv4 xfrm_user xfrm_algo nft_counter nft_log bridge stp llc vfio_pci vfio_pci_core cuse overlay rdma_ucm(OE) rdma_cm(OE) iw_cm(OE) ib_ipoib(OE) ib_cm(OE) ib_umad(OE) ascend_xsmen(OE) drv_dp_proc_mng_host(OE) drv_pcie_vnic_host(OE) drv_dvpp_cmdlist(OE) drv_soft_fault(OE) dbl_algorithm(OE) dbl_dev_identity(OE) ts_agent(OE) ascend_trs_shrid(OE) ascend_queue(OE) ascend_trs_sec_ch_agent(OE) ascend_trs_pm_adapt(OE) ascend_trs_core(OE) ascend_trs_sub_stars(OE) ascend_soc_platform(OE) drv_pcie_hdc_host(OE) ascend_event_sched_host(OE) ascend_trs_nvme_chan(OE) ascend_trs_id_allocator(OE) drv_devmm_host_agent(OE) sunrpc binfmt_misc nls_iso8859_1 drv_devmm_host(OE) ascend_dms_smf(OE) ascend_dms_dtm(OE) ascend_urd(OE) drv_virtmng_host(OE) ascend_soc_resmng(OE) drv_pcie_host(OE)
[54278.219384] drv_vpc_host(OE) ascend_uda(OE) drv_davinci_icnf_host(OE) ascend_logdrv(OE) dbl_runevn_config(OE) mlx5_ib(OE) drv_vascend(OE) mdev drv_seclib_host(OE) ib_uverbs(OE) drv_vascend_stub(OE) vfio_virqfd ipmi_ssif vfio_ipmi ipmi_devintf input_leds joydev ib_core(OE) vfio_iommu_type1 ipmi_msghandler sch_fq_codel bonding kvm(OE) efi_pstore ip_tables x_tables autofs4 xfs btrfs blake2b_generic zstd_compress raid10 raid456 async_raid6_recov async_memcpy async_pq async_xor async_tx xor neo n raid6_pq libcrc32c raid1 raid0 multipath linear dm_mirror dm_region_hash dm_log lpfc nvmet fc nvmet_fc nvme_fabrics scsi_transport_fc smartpqi mpt3sas raid_class aacraid ses enclosure hid_generic hibmc_drm drm_vram_helper drm_ttm_helper ttm i2c_algo_bit drm_kms_helper mlx5_core(OE) syscopyarea sysfillrect crct10dif_ce sysimgblt ghash_ce mlxdevm(OE) fb_sys_fops sha256_arm64 mlxfw(OE) sha1_ce psample cec hisi_sas_v3_hw nvme tls usbhid rc_core hisi_sas_main hns3 nvme_core crypto_simd cryptd aes_ce_cipher
[54278.370745] megaraid_sas(OE) mlx_nic_compat(OE) hclge dim libsas xhci_pci hnae3 xhci_pci_renesas ahci scsi_transport_sas spi_dw_mmio gpio_dwqapb spi_dw_nfit scsi_dh_emc scsi_dh_rdac scsi_dh_alua dm_multipath aes_neon_bs aes_neon_blk aes_ce_blk cr
[54278.431423] CPU: 49 PID: 275353 Comm: insmod Tainted: G        W  OE     5.15.0-25-generic #25-Ubuntu
[54278.431437] Hardware name: Yunke China KunTai A924/IT213K4C, BIOS 13.01.05 12/23/2023
[54278.484931] pstate: 20400009 (nzcv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[54278.484937] pc : mod_sysfs_setup+0x240/0x3c0
[54278.484941] lr : mod_sysfs_setup+0x2c0/0x3c0
[54278.458471] sp : ffff9b0cbfa7d878
[54278.458474] x29: ffff9b0c2446c858 x28: ffff9b0c24469ff0 x27: ffff9b0c24469ff0
[54278.458478] x26: ffff80089737000 x25: ffff800b0dacd000 x24: ffff800b07a86110
[54278.458481] x23: ffff9b0c2e230b0c x22: 7368727465a72782
[54278.585594] x21: 0000000000000000 x20: 050323bfa8c17bfd
[54278.585597] Call trace:
[54278.585600]  mod_sysfs_setup+0x240/0x3c0
[54278.585603]  load_module+0x9d8/0xbc0
[54278.585607]  do_sys_init_module+0xa8/0x400
[54278.585610]  __arm64_sys_finit_module+0x2c/0x40
[54278.585614]  invoke_syscall+0x50/0xe0
[54278.585618]  el0_svc_common.constprop.0+0x5c/0x184
[54278.585621]  do_el0_svc+0x60/0x80
[54278.585624]  el0_svc+0x64/0x130
[54278.585627]  el0t_64_sync_handler+0x4a/0x130
[54278.585630]  el0t_64_sync+0x190/0x1b0
[54278.597193] ---[ end trace 0649c022fe22029f ]---
```

## 根因定位

从提供的崩溃日志来看，发生了 **"Unable to handle kernel paging request at virtual address 0x00232bfa8c17cc5"**，这表明内核尝试访问了一个无效的地址，导致 **数据访问异常（Data Abort）**。

### **问题分析**
1. **异常类型**：
   ```
   ESR = 0x00000004
   ```
   - `ESR`（Exception Syndrome Register）的值 `0x4` 表示 **Level 0 Translation Fault**，即访问的地址没有有效的页表映射。

2. **故障地址**：
   ```
   Unable to handle kernel paging request at virtual address 0x00232bfa8c17cc5
   ```
   - 访问的地址 `0x00232bfa8c17cc5` 非常异常，看起来像是 **一个损坏的指针** 或 **野指针**，可能来源于：
     - **空指针解引用（NULL Pointer Dereference）**
     - **非法地址访问**
     - **释放后使用（Use-After-Free, UAF）**
     - **未正确初始化指针**

3. **地址空间错误**：
   ```
   Data abort info:
   ISV = 0, ISS = 0x4, CM = 0, WnR = 0
   ```
   - **WnR = 0**：指示**是读取（Read）操作**，说明代码尝试从 `0x00232bfa8c17cc5` 读取数据时出错。
   - **数据地址不在内核地址范围**：
     ```
     [00232bfa8c17cc5] address between user and kernel address ranges
     ```
     - 这个地址不在用户空间或典型的内核空间范围内，可能是因为：
       - 指针被破坏，导致访问了无效地址
       - 结构体指针被错误解析

4. **调用栈（Call Trace）**：
   ```
   Call trace:
   mod_sysfs_setup+0x240/0x3c0
   load_module+0x9d8/0xbc0
   do_sys_init_module+0xa8/0x400
   ...
   el0t_64_sync_handler+0x4a/0x130
   ```
   - `mod_sysfs_setup` 相关函数出现在调用栈中，表明可能是 **内核模块加载** 过程中出现的问题。
   - `load_module` 说明 **该问题可能与插入的内核模块（.ko 文件）相关**。

5. **涉及的模块（Modules linked in）**
   ```
   Modules linked in: tramme1(OE) xt_conntrack ...
   ```
   - 其中 **"tramme1(OE)"** 可能是一个第三方内核模块（带有 `(OE)` 表示非 GPL 模块），问题可能与它的加载过程相关。

---

### **可能的原因**
1. **内核模块 `tramme1(OE)` 可能访问了无效指针**
   - 可能 `mod_sysfs_setup` 处理 `sysfs` 相关操作时，模块传递了错误的指针或数据结构，导致访问了无效地址。

2. **Use-After-Free（UAF）或者野指针**
   - 如果模块之前释放了某个结构体但仍然访问它，就可能会出现这种情况。
   - 可能由于竞态条件或错误的资源管理导致。

3. **用户态地址在内核空间使用**
   - `0x00232bfa8c17cc5` 既不是典型的用户态地址，也不是合法的内核地址，可能是某个用户态地址被错误地用于内核访问。

---

### **如何排查**
1. **检查 `tramme1(OE)` 模块的代码**
   - 重点查看 **sysfs 相关的 `mod_sysfs_setup`** 代码。
   - 确保 **没有解引用空指针或释放后的指针**。

2. **使用 `dmesg` 或 `ftrace` 追踪模块的加载过程**
   ```
   dmesg | grep tramme1
   ```
   ```
   trace-cmd record -e module
   ```

3. **开启 KASAN（Kernel Address Sanitizer）**
   - 如果可以重新编译内核，建议启用 KASAN 以检查内存非法访问。

4. **尝试 `addr2line` 确定具体出错的代码**
   - 例如：
     ```
     addr2line -e /path/to/kernel/vmlinux 0xmod_sysfs_setup+0x240
     ```
   - 这将帮助定位出错的内核源代码行。

---

### **结论**
- **怀疑 `tramme1(OE)` 内核模块在 `mod_sysfs_setup` 过程中访问了无效地址**，可能是野指针或者 `Use-After-Free` 问题。

```bash
0xffffd44ebe81f09c <mod_sysfs_setup+0x21c>:     b.ne    0xffffd44ebe81f0b4 <mod_sysfs_setup+556>  // b.any
                                                      // 如果不相等（非零）则跳转到 0xffffd44ebe81f0b4
0xffffd44ebe81f0a0 <mod_sysfs_setup+0x220>:     b       0xffffd44ebe81f168 <mod_sysfs_setup+0x2e8>
                                                      // 无条件跳转到 0xffffd44ebe81f168
0xffffd44ebe81f0a4 <mod_sysfs_setup+0x224>:     ldr     x0, [x21, #16]
                                                      // 从 x21 + 16 地址加载数据到 x0
0xffffd44ebe81f0a8 <mod_sysfs_setup+0x228>:     sub     x21, x0, #0x10
                                                      // x21 = x0 - 0x10
0xffffd44ebe81f0ac <mod_sysfs_setup+0x22c>:     cmp     x26, x0
                                                      // 比较 x26 和 x0
0xffffd44ebe81f0b0 <mod_sysfs_setup+0x230>:     b.eq    0xffffd44ebe81f168 <mod_sysfs_setup+736>  // b.none
                                                      // 如果相等则跳转到 0xffffd44ebe81f168
/build/linux-44SzGa/linux-5.15.0/kernel/module.c: 1737
0xffffd44ebe81f0b4 <mod_sysfs_setup+0x234>:     ldr     x0, [x21, #40]
                                                      // 从 x21 + 40 地址加载数据到 x0
0xffffd44ebe81f0b8 <mod_sysfs_setup+0x238>:     mov     x2, x24
                                                      // x2 = x24
0xffffd44ebe81f0bc <mod_sysfs_setup+0x23c>:     mov     x1, x23
                                                      // x1 = x23
0xffffd44ebe81f0c0 <mod_sysfs_setup+0x240>:     ldr     x0, [x0, #200]
                                                      // 从 x0 + 200 地址加载数据到 x0
0xffffd44ebe81f0c4 <mod_sysfs_setup+0x244>:     bl      0xffffd44ebeb72a80 <sysfs_create_link>
                                                      // 调用 sysfs_create_link 函数
0xffffd44ebe81f0c8 <mod_sysfs_setup+0x248>:     mov     w20, w0
                                                      // w20 = w0 (保存返回值)
/build/linux-44SzGa/linux-5.15.0/kernel/module.c: 1739
0xffffd44ebe81f0cc <mod_sysfs_setup+0x24c>:     cbz     w0, 0xffffd44ebe81f0a4 <mod_sysfs_setup+0x224>
                                                      // 如果 w0 为零则跳转到 0xffffd44ebe81f0a4
/build/linux-44SzGa/linux-5.15.0/kernel/module.c: 1742
0xffffd44ebe81f0d0 <mod_sysfs_setup+0x250>:     add     x21, x27, #0x40
                                                      // x21 = x27 + 0x40
0xffffd44ebe81f0d4 <mod_sysfs_setup+0x254>:     mov     x0, x21
                                                      // x0 = x21
0xffffd44ebe81f0d8 <mod_sysfs_setup+0x258>:     bl      0xffffd44ebf751150 <mutex_unlock>
                                                      // 调用 mutex_unlock 函数
/build/linux-44SzGa/linux-5.15.0/kernel/module.c: 1722
0xffffd44ebe81f0dc <mod_sysfs_setup+0x25c>:     mov     x0, x21
                                                      // x0 = x21
```

从反汇编角度来看，解引用use->target->holders_dir出了问题:
```c
// vim kernel/module.c +1837

1735     mutex_lock(&module_mutex);
1736     list_for_each_entry(use, &mod->target_list, target_list) {
1737         ret = sysfs_create_link(use->target->holders_dir,
1738                     &mod->mkobj.kobj, mod->name);
1739         if (ret)
1740             break;
```

kernel/module.c +1720 struct module_use *use初始化：
```bash
init_module
   -> load_module
         -> simplify_symbols
               -> resolve_symbol_wait
                     -> resolve_symbol
                           -> ref_module
                                 -> add_module_usage
```

kernel/module.c +1737 use->target->holders_dir初始化：
```c
// vim kernel/module.c +1856

845 static int mod_sysfs_setup(struct module *mod,
1846                const struct load_info *info,
1847                struct kernel_param *kparam,
1848                unsigned int num_params)
1849 {
1850     int err;
1851
1852     err = mod_sysfs_init(mod);
1853     if (err)
1854         goto out;
1855
1856     mod->holders_dir = kobject_create_and_add("holders", &mod->mkobj.kobj);
1857     if (!mod->holders_dir) {
1858         err = -ENOMEM;
1859         goto out_unreg;
1860     }
```
