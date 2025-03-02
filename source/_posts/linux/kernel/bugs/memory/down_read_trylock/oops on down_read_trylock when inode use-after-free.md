---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# oops on down_read_trylock when inode use-after-free

## oops log

```bash
[454096.703799] Unable to handle kernel NULL pointer dereference at virtual address 00000000000000a0
[454096.706827] Mem abort info:
[454096.707584]   ESR = 0x96000004
[454096.708454]   Exception class = DABT (current EL), IL = 32 bits
[454096.710046]   SET = 0, FnV = 0
[454096.710947]   EA = 0, S1PTW = 0
[454096.711639] Data abort info:
[454096.712367]   ISV = 0, ISS = 0x00000004
[454096.713472]   CM = 0, WnR = 0
[454096.715424] user pgtable: 4k pages, 48-bit VAs, pgdp = 000000003bd839ba
[454096.717782] [00000000000000a0] pgd=0000000000000000
[454096.719010] Internal error: Oops: 96000004 [#1] SMP
[454096.720102] Modules linked in: arc4 md4 sha512_generic sha512_arm64 nls_utf8 cifs(O) ccm dns_resolver uvcvideo videobuf2_vmalloc videobuf2_memops videobuf2_v4l2 videobuf2_common videodev media uinput nfnetlink_queue nfnetlink_log nfnetlink fuse dlp_fcore(O) rpcsec_gss_krb5 auth_rpcgss nfsv3 nfs_acl nfs lockd grace sunrpc bnep st bluetooth ecdh_generic rfkill vfs_monitor(O) clink_vhci_hcd(O) clink_usbip_core(O) nls_iso8859_1 nls_cp437 aes_ce_blk crypto_simd cryptd aes_ce_cipher crct10dif_ce ghash_ce aes_arm64 sha2_ce sha256_arm64 sha1_ce snd_intel8x0 snd_ac97_codec virtio_balloon qemu_fw_cfg uos_resources(O) uos_bluetooth_connection_control(O) rdma_cm iw_cm ib_cm ib_core efivarfs ip_tables x_tables hid_generic usbkbd usbhid btrfs xor raid6_pq virtio_blk virtio_net net_failover failover button virtio_mmio
[454096.740898] Process JobController (pid: 8151, stack limit = 0x00000000374c32bd)
[454096.742812] CPU: 2 PID: 8151 Comm: JobController Tainted: G        W  O      4.19.0-arm64-desktop #5312
[454096.745536] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
[454096.747064] pstate: 60400005 (nZCv daif +PAN -UAO)
[454096.748751] pc : down_read_trylock+0x28/0x58
[454096.750076] lr : d_splice_alias+0x290/0x4b8
[454096.751318] sp : ffff8000dfe63ab0
[454096.752201] x29: ffff8000dfe63ab0 x28: ffff800026229a00 
[454096.753820] x27: 0000000000000002 x26: ffff800393b35420 
[454096.755426] x25: ffff8003e444b480 x24: ffff80025bb1d8f0 
[454096.756860] x23: ffff000009809000 x22: ffff80025bb1d840 
[454096.758665] x21: ffff0000098094c0 x20: ffff80029d64ea60 
[454096.760103] x19: ffff80000c873180 x18: 0000000000000000 
[454096.761446] x17: 0000000000000000 x16: 0000000000000000 
[454096.762932] x15: 0000000000000000 x14: ffff8003ff6a1ea0 
[454096.764253] x13: ffff00000911ccd0 x12: 00000000009dbb18 
[454096.765636] x11: 0000000000000016 x10: 0000000000000004 
[454096.767072] x9 : 0000000000000020 x8 : 0000000000000020 
[454096.768909] x7 : 0000000000001000 x6 : 0000000000c75c20 
[454096.770430] x5 : 0000000000000000 x4 : 0000000000000000 
[454096.772030] x3 : ffff800026229a00 x2 : 00000000000000a0 
[454096.773443] x1 : 0000000000000000 x0 : 00000000000000a0 
[454096.774858] Call trace:
[454096.775548]  down_read_trylock+0x28/0x58
[454096.776798]  ext4_lookup+0x16c/0x228
[454096.777765]  __lookup_slow+0x78/0x168
[454096.778691]  lookup_slow+0x3c/0x60
[454096.779863]  walk_component+0x1e4/0x2e0
[454096.780820]  path_lookupat.isra.12+0x5c/0x1e0
[454096.781935]  filename_lookup.part.20+0x6c/0x110
[454096.783110]  user_path_at_empty+0x4c/0x60
[454096.784170]  do_faccessat+0x8c/0x220
[454096.785119]  __arm64_sys_faccessat+0x1c/0x28
[454096.786269]  el0_svc_common+0x90/0x178
[454096.787190]  el0_svc_handler+0x9c/0xa8
[454096.788099]  el0_svc+0x8/0xc
[454096.788832] Code: c8047c40 35ffff84 eb01007f 540000c0 (f9400041) 
[454096.790634] ---[ end trace 9972b2691eefd1ea ]---
```

## 初步排查

```bash
gdb vmlinux
p ((struct inode *)0)->i_rwsem
Cannot access memory at address 0xa0
```

inode 虚拟地址为: `0x00000000000000a0` - `0xa0` = 0x0000000000000000`，说明inode是null指针，这种情况倾向于inode use-after-free。

当前已启用kasan，编译出内核tyy-5312-kasan.tar.gz，等待kasan报告。
