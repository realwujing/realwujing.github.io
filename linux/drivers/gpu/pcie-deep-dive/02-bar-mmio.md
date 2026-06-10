# 第二篇：BAR/MMIO — 设备的寄存器窗口

> 源码：`drivers/pci/setup-res.c` `drivers/pci/setup-bus.c` | 头文件：`include/linux/pci.h` `drivers/pci/pci.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. 概述

枚举发现了设备，但 CPU 还不能和设备通信。设备有自己的内部寄存器（Doorbell、Command Queue、Status Registers...），需要通过 BAR（Base Address Register）映射到 CPU 的地址空间。

本文聚焦：
- BAR 的硬件原理：Type 0（Endpoint）vs Type 1（Bridge）
- 内核如何读 BAR、写 BAR、分配地址
- 64-bit BAR、Prefetchable BAR
- GPU 的大 VRAM BAR 布局

---

## 2. BAR 硬件概述

### 2.1 配置空间中的 BAR 寄存器

Type 0 Header（Endpoint 设备）在配置空间 offset 0x10–0x24 有 **6 个 BAR**：

```
Offset  Type 0 (Endpoint)          Type 1 (PCI-to-PCI Bridge)
------  -------------------------  ---------------------------
 0x10   BAR0                        BAR0
 0x14   BAR1                        BAR1
 0x18   BAR2                        I/O Base / Limit (bridge)
 0x1C   BAR3                        Memory Base / Limit
 0x20   BAR4                        Prefetchable Memory Base / Limit
 0x24   BAR5                        Prefetchable Base Upper 32 bits
```

- **Endpoint**（Type 0）：6 个 BAR，可混合 IO / Memory / 64-bit
- **Bridge**（Type 1）：2 个 BAR（自身寄存器），其余字段用于桥窗口

### 2.2 BAR 寄存器位布局

```
Memory BAR (bit[0]==0):
 31                       4  3  2  1  0
+--------------------------+-----+--+--+
|    Base Address [31:4]   | 预取 | T |0|
+--------------------------+-----+--+--+
                                 |
                           bits[2:1]=00: 32-bit
                           bits[2:1]=10: 64-bit

I/O BAR (bit[0]==1):
 31                       2  1  0
+--------------------------+--+--+
|    Base Address [31:2]   | R |1|
+--------------------------+--+--+
                          R=保留
```

- **bit[0]**：0 = Memory BAR，1 = I/O BAR
- **Memory BAR bits[2:1]**：00 = 32-bit addressable, 10 = 64-bit addressable
- **bit[3]**：Memory BAR 的 prefetchable 位（仅 Memory BAR 有意义）
- 低位的属性位是**只读**的，写 1 无效果 — 内核正是利用这个特性来探测 BAR 大小

### 2.3 探测 BAR 大小的方法

```
Step 1: 读 BAR，保存 original = bar_value
Step 2: 写 BAR = 0xFFFFFFFF
Step 3: 读 BAR，sized = bar_value
Step 4: size = ~(sized & mask) + 1
Step 5: 写回 original
```

属性位（bit[0:3]）写 1 会被硬件忽略，读回来永远是 0。所以写入 0xFFFFFFFF 后读回的值中，低位的属性位为 0，高位为 1 的是可编程位，取反加 1 就是 BAR 大小。

例如：写 0xFFFFFFFF → 读回 0xFFF00000 → 大小 = ~0xFFF00000 + 1 = 0x00100000 = 1 MiB。

---

## 3. 内核中的 BAR 表示

### 3.1 枚举类型：drivers/pci/pci.h:484-487

```c
// pci.h (internal):484-487
enum pci_bar_type {
    pci_bar_unknown,    /* Standard PCI BAR probe */
    pci_bar_io,         /* An I/O port BAR */
    pci_bar_mem32,      /* A 32-bit memory BAR */
    pci_bar_mem64,      /* A 64-bit memory BAR */
};
```

这个枚举用于 `__pci_read_base()` 在 `probe.c` 中解析 BAR 后分类。

### 3.2 struct pci_dev.resource[] — include/linux/pci.h:454

```c
// pci.h:454
struct resource resource[DEVICE_COUNT_RESOURCE];
```

`DEVICE_COUNT_RESOURCE` 在 `include/linux/pci.h` 中定义（>17），包括：

| 索引 | 含义 |
|------|------|
| 0–5 | BAR0–BAR5 |
| 6 | ROM BAR |
| 7–12 | SR-IOV BAR（VF 用）|
| 13–15 | Bridge 窗口（IO / Mem / Prefetch） |
| 16 | Bridge 窗口 64-bit 高半部分 |

### 3.3 resource flags — IORESOURCE_IO vs IORESOURCE_MEM

```c
// I/O BAR:
resource[0].flags = IORESOURCE_IO

// Memory BAR:
resource[0].flags = IORESOURCE_MEM      // 32-bit 或 64-bit
resource[0].flags |= IORESOURCE_MEM_64  // 如果是 64-bit
resource[0].flags |= IORESOURCE_PREFETCH // 如果 prefetchable
```

判断逻辑在 `__pci_read_base()`（`probe.c` 中）：
```c
// 读 BAR 的 bit[0] 决定是 IO 还是 MEM
if (l & PCI_BASE_ADDRESS_SPACE_IO)    // bit[0] == 1
    type = IORESOURCE_IO;
else
    type = IORESOURCE_MEM;
```

---

## 4. 64-bit BAR — 两个连续槽位

64-bit BAR 占用**两个连续的 BAR 槽位**。例如 BAR1 设为 64-bit，则 BAR2 不可用，BAR1+2 拼接成 64 位地址：

```
+----------+----------+
|  BAR1    |  BAR2    |
| addr[31:0]|addr[63:32]|
+----------+----------+
```

在 `__pci_read_base()` 中：
```c
if (type == pci_bar_mem64) {
    // 读高 32 位
    pci_read_config_dword(dev, pos + 4, &sz);
    region.start |= ((u64)sz) << 32;
    region.end |= ((u64)sz) << 32;
    // 跳过下一个 BAR 索引
    bar++;
}
```

这意味着实际可用的 BAR 数量会因 64-bit BAR 的占用而减少。

---

## 5. pci_std_update_resource — 写 BAR

`drivers/pci/setup-res.c:25-60`：

```c
// setup-res.c:25-60
static void pci_std_update_resource(struct pci_dev *dev, int resno)
{
    struct pci_bus_region region;
    bool disable;
    u16 cmd;
    u32 new, check, mask;
    int reg;
    struct resource *res = pci_resource_n(dev, resno);
    const char *res_name = pci_resource_name(dev, resno);

    /* Per SR-IOV spec 3.4.1.11, VF BARs are RO zero */
    if (dev->is_virtfn)
        return;

    /*
     * Ignore resources for unimplemented BARs and unused resource slots
     * for 64 bit BARs.
     */
    if (!res->flags)
        return;                          // BAR 未实现或已废弃

    if (res->flags & IORESOURCE_UNSET)
        return;                          // 尚未分配地址

    /*
     * Ignore non-moveable resources.  This might be legacy resources for
     * which no functional BAR register exists or another important
     * system resource we shouldn't move around.
     */
    if (res->flags & IORESOURCE_PCI_FIXED)
        return;

    pcibios_resource_to_bus(dev->bus, &region, res);
    new = region.start;

    if (res->flags & IORESOURCE_IO) {
        mask = (u32)PCI_BASE_ADDRESS_IO_MASK;       // I/O BAR mask
        new |= res->flags & ~PCI_BASE_ADDRESS_IO_MASK;
    } else if (resno == PCI_ROM_RESOURCE) {
        mask = PCI_ROM_ADDRESS_MASK;                 // ROM BAR mask
    // ...
```

流程：

1. 跳过 VF（Virtual Function）的 BAR — VF 的 BAR 是只读零
2. 跳过未实现 / 未分配 / 固定资源
3. `pcibios_resource_to_bus()` 将 CPU 物理地址转换为 PCI 总线地址（大多数平台两者相同，但某些平台有偏移）
4. 根据 `res->flags` 区分 I/O 和 Memory 路径
5. 写回 BAR 寄存器

后续（L65+）：将计算好的 `new` 值写入配置空间对应偏移，再读回验证，确保写入成功。

```
写 BAR 示意:
  CPU addr: 0x3800_0000_0000  →  pcibios_resource_to_bus  →  bus addr: 0x38_0000_0000
                                                                    (64-bit BAR)
  pci_write_config_dword(dev, BAR0_offset, bus_addr_lower32);
  pci_write_config_dword(dev, BAR1_offset, bus_addr_upper32);
```

---

## 6. Prefetchable vs Non-Prefetchable

内核对待 prefetchable 和 non-prefetchable 内存的策略完全不同：

| 属性 | Non-Prefetchable | Prefetchable |
|------|-----------------|--------------|
| 读副作用 | 有（如 FIFO 寄存器） | 无（如 VRAM） |
| CPU 可合并读 | 否 | 是 |
| WC 映射可能 | 否 | 是（ioremap_wc） |
| 分配策略 | 低 4GB 优先 | 无所谓，可放 >4GB |
| 典型用途 | MMIO 寄存器 | 显存、网卡队列内存 |

在 `pci_bridge_check_ranges()`（setup-bus.c 中）中检查桥是否支持 prefetchable 窗口以及是否 64-bit。

GPU 的 VRAM BAR 一定是 **64-bit prefetchable**，因为：
- VRAM > 4GB，必须 64-bit
- VRAM 读取无副作用，CPU 可以预取/合并读

---

## 7. GPU VRAM BAR 布局实例

以一张典型 GPU（如 NVIDIA A100 80GB）为例：

```
        +------------------------------------------------+
        |               BAR0: 32MB Non-Pref              |
        |               (MMIO 寄存器窗口)                 |
        +------------------------------------------------+
        |               BAR1: 64GB Prefetchable 64-bit   |
        |               (VRAM 窗口 — 占用 BAR1+BAR2)      |
        +------------------------------------------------+
        |               BAR3: 32MB Prefetchable          |
        |               (NVIDIA 内部使用)                  |
        +------------------------------------------------+
        BAR4, BAR5: 未使用

lspci -v 输出:
  Region 0: Memory at a1000000 (32-bit, non-prefetchable) [size=32M]
  Region 1: Memory at 38000000000 (64-bit, prefetchable) [size=64G]
  Region 3: Memory at a3000000 (32-bit, prefetchable) [size=32M]
```

内核中对应的 `resource[]` 数组：

```
resource[0].start = 0x00000000a1000000, .end = 0x00000000a2ffffff, .flags = IORESOURCE_MEM
resource[1].start = 0x0000038000000000, .end = 0x0000038fffffffff,
    .flags = IORESOURCE_MEM | IORESOURCE_MEM_64 | IORESOURCE_PREFETCH
resource[2].flags = 0   // 被 BAR1 的 64-bit 高半部分占用，resource slot 置空
resource[3].start = 0x00000000a3000000, .end = 0x00000000a4ffffff,
    .flags = IORESOURCE_MEM | IORESOURCE_PREFETCH
```

### 7.1 Above-4G 解码

GPU 的 64GB VRAM BAR 必须放在 4GB 以上的地址空间。这要求：
1. **Root Complex / Host Bridge** 支持 above-4G 解码（ACPI 中 `_CRS` 描述 64-bit 窗口）
2. **GPU 自身的 command register** 中 memory decode enable 位被置 1
3. 内核 `pci_setup_bridge()` 正确设置了桥的 memory base/limit 寄存器

如果 ACPI 没有在 `_CRS` 中声明 64-bit 窗口，内核无法分配 >4GB 地址，64GB VRAM 就映射不进去，设备无法工作。

---

## 8. Command Register — 控制 BAR 解码

BAR 分配好了地址，还需要在 command 寄存器中打开 decode：

```
PCI Command Register (offset 0x04):
  bit[0] = IO Space Enable      — 允许响应 I/O 访问
  bit[1] = Memory Space Enable  — 允许响应 Memory 访问  ★ 最关键
  bit[2] = Bus Master Enable    — 允许发起 DMA  ★ DMA 必须打开
```

内核在 `pci_std_update_resource` 末尾会操作 command register：如果需要 disable 就清零相应位，写入新 BAR 地址后再置回。

---

## 9. 桥的 BAR 窗口 — 不同的故事

Type 1 设备的 BAR 只有 2 个，其余部分用于描述"这个桥能给下游分配什么范围"：

```
Bridge Config Space:
  0x1C  I/O Base / I/O Limit     — 下游 I/O 窗口
  0x20  Memory Base / Memory Limit — 下游 32-bit Memory 窗口
  0x24  Prefetchable Memory Base / Prefetchable Memory Limit
  0x28  Prefetchable Base Upper 32   — 64-bit prefetch 窗口高半部分
  0x2C  Prefetchable Limit Upper 32
```

桥的窗口定义了下游总线可见的地址范围。如果桥不支持 prefetchable 窗口（`pref_window=0`），GPU 的 VRAM BAR 就无法通过这个桥分配。

内核在 `pci_bridge_check_ranges()` 中探测桥的能力：
```c
// 写 Base/Limit 寄存器测试是否可写
pci_write_config_dword(dev, PCI_PREF_MEMORY_BASE, ...);
pci_read_config_dword(dev, PCI_PREF_MEMORY_BASE, &base);
if (base != 0)
    dev->pref_window = 1;      // 支持 prefetchable 窗口
```

---

## 10. 从 probe 到 BAR 赋值的时间线

```
枚举阶段:  pci_scan_device → pci_setup_device → __pci_read_base
           (读 BAR 大小和类型，不分配地址)

资源分配:  pci_assign_unassigned_resources → pci_bus_assign_resources
           → 遍历 iomem_resource 树，找到空闲区间，赋值给各 BAR
           → 更新 resource[n].start / .end

BAR 写入:  pci_std_update_resource
           → 将 resource[n].start 写入 BAR 寄存器
           → 设置 command register 的 memory enable 位
```

---

## 11. 总结

| 问题 | 答案 |
|------|------|
| BAR 怎么探测大小？ | 写 0xFFFFFFFF 再读回，取反加 1 |
| I/O vs Memory BAR 怎么区分？ | 读 bit[0]，1=I/O，0=Memory |
| 64-bit BAR 怎么处理？ | 占两个 BAR 槽位，高 32 位在下一个寄存器 |
| GPU VRAM 为什么是 prefetchable？ | VRAM 读取无副作用，允许 CPU 合并/预取 |
| 什么时候写 BAR？ | 资源分配完成后，`pci_std_update_resource` |

### GPU VRAM BAR 实战案例分析

以一块典型的高端 GPU（如 RTX 4090 24GB）为例，在内核日志中可以看到：

```
pci 0000:01:00.0: [10de:2684] type 00 class 0x030000
pci 0000:01:00.0: reg 0x10: [mem 0x8800000000-0x880fffffff 64bit pref]
pci 0000:01:00.0: reg 0x1c: [mem 0x8810000000-0x8811ffffff 64bit pref]
```

- **BAR0 (reg 0x10)**: 256MB, 64-bit prefetchable — 这是 GPU 的 MMIO 寄存器窗口。CPU 通过这片空间访问 GPU 的 MMIO 寄存器（如 doorbell、fence、中断控制等）
- **BAR3 (reg 0x1c)**: 32MB, 64-bit prefetchable — 典型的是帧缓冲区或配置空间
- **VRAM 大 BAR**（通过 Resizable BAR 可达 16GB+）: 64-bit prefetchable，BIOS 初始分配较小（256MB），驱动通过 `pci_rebar_set_size` 协商扩展到实际需要的大小

Resizable BAR（ReBAR）初始化流程在 `pci_rebar_init()`（probe.c:2655 的 `pci_init_capabilities` 中第 20 步调用，line 2679）。驱动通过 `pci_resize_resource()` 写入新的 resource size 到 ReBAR capability 寄存器。

### bridge 窗口分配 — pci_scan_bridge_extend

对于 PCIe Switch/Root Port，bridge 有自己的 BAR 窗口（bridge resource windows），通过 `pci_scan_bridge_extend()` 分配：

```
Root Port
  ├── Primary Bus: 0
  ├── Secondary Bus: 1
  ├── Subordinate Bus: 5
  ├── MMIO Window:  [0x8800000000, 0x881FFFFFFF]  ← downstream MMIO
  └── Prefetchable Window: [0x8820000000, 0x8BFFFFFFFF]  ← downstream prefetch

Switch Upstream Port → Downstream Port(s)
  └── 每个 downstream 再分配子窗口
```

`pbus_validate_busn`（probe.c:1321-1338）校验 bridge 的 bus number 范围是否被上游 bridge 窗口覆盖——如果不在范围内，bridge 下的所有设备都无法访问。

### 内核中的内存域表示

`struct pci_bus.resource[4]` 对应 bridge 的 4 类资源窗口（`include/linux/pci.h:700-735` 的 bus 结构体）：

| 索引 | 类型 | 用途 |
|------|------|------|
| 0 | I/O Port | 传统 I/O 空间（legacy 设备） |
| 1 | Memory | 非prefetchable MMIO window |
| 2 | Prefetchable Memory | prefetchable MMIO window |
| 3 | 64-bit Prefetchable | above-4G prefetchable window |

BAR 和 MMIO 是驱动读写设备寄存器的**唯一通道**。下一篇文章将讨论驱动如何通过 `pci_driver` 注册并获得已设置好的 `pci_dev`。

---

## 下一篇文章

[第三篇：驱动注册与匹配](./03-driver-probe.md)
