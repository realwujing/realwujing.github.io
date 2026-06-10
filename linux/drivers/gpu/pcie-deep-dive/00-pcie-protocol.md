# 第0篇：PCIe 协议 — 从配置空间到 TLP 的完整图谱

> 源码：`include/uapi/linux/pci_regs.h` `drivers/pci/pci.h` `drivers/pci/ecam.c` | 系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 为什么要先讲协议

Linux 内核的 PCIe 代码本质上是对 PCIe 协议规范的软件实现。不懂配置空间的寄存器布局，就看不懂 `pci_scan_device` 在读什么；不懂 Capability 链表的串联方式，就看不懂 `pci_init_capabilities` 在初始化什么；不懂 TLP 的格式，就看不懂 AER 日志和 MSI 消息。

本文不讲完整的 PCIe 6.0 规范（1000+ 页），而是聚焦内核代码中实际用到的**协议核心概念**。

---

## 1. 配置空间：设备的"护照"

每个 PCIe 设备在硬件上都有一段**配置空间**（Configuration Space），存储该设备的身份信息、资源需求、能力声明。内核通过读这段空间识别和配置设备。

### 1.1 两种大小

```c
// include/uapi/linux/pci_regs.h:29-30
#define PCI_CFG_SPACE_SIZE      256     // 标准 PCI 配置空间（前 256 字节）
#define PCI_CFG_SPACE_EXP_SIZE  4096    // PCIe 扩展配置空间（扩展到 4KB）
```

- **前 256 字节**：PCI 兼容（传统 PCI 设备也支持），包含 Vendor ID、Device ID、BAR、中断等
- **256 字节 ~ 4KB**：PCIe 扩展空间（Extended Configuration Space），存放 PCIe 扩展能力（Extended Capabilities）

> 区分两个概念：Standard Capability（标准能力，链头在 0x34）位于前 256 字节；Extended Capability（扩展能力）位于 256~4KB 区域。

### 1.2 如何访问配置空间

x86 传统方式是通过 I/O 端口：写 0xCF8 指定 Bus/DevFn/Offset，读 0xCFC 获取数据。但现代 ARM64 和 x86 大内存系统都用 **ECAM**（Enhanced Configuration Access Mechanism）——把整个配置空间映射为 MMIO：

```
ECAM 地址 = ECAM基址 + (Bus号 << 20) + (DevFn号 << 12) + Offset

Bus号:     0-255 (8位)
DevFn:     0-31 dev, 0-7 func, 高位是 dev
Offset:    0-4095 (12位, 每个设备 4KB)
```

ECAM 映射在内核中的入口：

```c
// ecam.c:27-54
struct pci_config_window *pci_ecam_create(struct device *dev,
        struct resource *cfgres, struct resource *busr,
        const struct pci_ecam_ops *ops)
{
    // 计算每个 bus 需要多少空间：
    // bus_range_max = resource_size(cfgres) >> bus_shift;
    //
    // 64-bit 系统：一次 ioremap 整个 ECAM 区域
    // 32-bit 系统：逐 bus ioremap
    cfg = kzalloc_obj(*cfg);
    cfg->busr.start = busr->start;     // bus 起始号
    cfg->busr.end   = busr->end;       // bus 结束号
    cfg->ops        = ops;             // ECAM ops (bus_shift, read/write)
    cfg->bus_shift  = bus_shift;       // 每个 bus 的地址偏移位数
}
```

### 1.3 前 256 字节的布局（Type 0 — 普通设备）

内核把这些偏移量定义在 `include/uapi/linux/pci_regs.h` 中：

```
Off  Size  Register                 内核宏                        Line
─────────────────────────────────────────────────────────────────────
0x00  4B   Vendor ID (低16)         PCI_VENDOR_ID                (line 42)
           Device ID (高16)         PCI_DEVICE_ID                (line 43)
0x04  4B   Command + Status         PCI_COMMAND                  (line 46)
0x08  4B   Revision ID (低8)        PCI_REVISION_ID              (line 58)
           Class Code (高24)        PCI_CLASS_PROG/DEVICE/BASE   (line 60-66)
0x0C  1B   Cache Line Size          PCI_CACHE_LINE_SIZE           (line 71)
0x0E  1B   Header Type              PCI_HEADER_TYPE              (line 78)
0x10  24B  BAR0 ~ BAR5              PCI_BASE_ADDRESS_0..5        (line 87-101)
0x2C  4B   Subsystem Vendor/Device  PCI_SUBSYSTEM_VENDOR_ID      (line 116-117)
0x30  4B   Expansion ROM            PCI_ROM_ADDRESS              (line 118)
0x34  1B   Capabilities Pointer     PCI_CAPABILITY_LIST          (line 122)
0x3C  1B   Interrupt Line           PCI_INTERRUPT_LINE           (line 125)
0x3D  1B   Interrupt Pin            PCI_INTERRUPT_PIN            (line 126)
```

### 1.4 Header Type — 设备的角色

```c
// pci_regs.h:78-83
#define PCI_HEADER_TYPE         0x0e    /* 8 bits */
#define  PCI_HEADER_TYPE_MASK   0x7f    /* 低 7 位是类型 */
#define  PCI_HEADER_TYPE_NORMAL  0      /* Type 0: 普通设备（GPU, NIC, NVMe） */
#define  PCI_HEADER_TYPE_BRIDGE  1      /* Type 1: PCI-to-PCI Bridge */
#define  PCI_HEADER_TYPE_CARDBUS 2      /* Type 2: CardBus Bridge */
#define  PCI_HEADER_TYPE_MFD     0x80   /* bit 7: 多功能设备 */
```

Type 0（普通设备）有 6 个 BAR，Type 1（bridge）只有 2 个 BAR + 总线号窗口。内核在 `pci_setup_device`（probe.c:2030）中读完 `hdr_type` 后据此分发后续配置。

### 1.5 Class Code — 设备的种类

```c
// pci_regs.h:58-66 定义了 Class Code 的三级结构
// offset 0x08:
//   [7:0]    Revision ID
//   [15:8]   Programming Interface
//   [23:16]  Sub-Class
//   [31:24]  Base Class
```

几个关键 Base Class 值：

| Base Class | 含义 | 典型设备 |
|-----------|------|---------|
| 0x03 | Display Controller | GPU（不管集成还是独显） |
| 0x02 | Network Controller | NIC、RDMA 网卡 |
| 0x01 | Mass Storage Controller | NVMe、SATA |
| 0x06 | Bridge Device | PCIe Switch / Root Port |

--- 

## 2. Capability 链表：设备的"功能声明"

PCI 配置空间里最精巧的设计之一就是 Capability 链表。没有它，每个设备功能需要固定位置存放，无法扩展。

### 2.1 Standard Capability 链表

```
配置空间偏移 0x34 存储第一个 capability 的偏移
    │
    ▼
┌──────────────────────────────┐
│ Off: 0x40                    │
│ [7:0]  = Capability ID (如 0x10=PCIe)      │
│ [15:8] = 指向下一个 cap 的偏移 (如 0x60)     │
├──────────────────────────────┤
│ ... capability 数据 (MSI, PM 等) ...         │
└──────────────────────────────┘
    │
    ▼
┌──────────────────────────────┐
│ Off: 0x60                    │
│ [7:0]  = 0x05 (MSI)         │
│ [15:8] = 0x80               │
├──────────────────────────────┤
│ ... MSI capability 数据 ...   │
└──────────────────────────────┘
    │
    ▼   ... 直到 Capability ID = 0xFF 或 Next Ptr = 0
```

内核的遍历宏（`pci.h:137-169`）：

```c
#define PCI_FIND_NEXT_CAP(read_cfg, start, cap, prev_ptr, args...) \
({                                                                  \
    int __ttl = PCI_FIND_CAP_TTL;        /* TTL=48, 防止死循环 */   \
    u8  __pos = (start);                                            \
    while (__ttl--) {                                               \
        if (__pos < 0x40)                    /* 低于标准 header */  \
            break;                                                  \
        __pos = ALIGN_DOWN(__pos, 4);                               \
        read_cfg##_word(args, __pos, &__ent);                       \
        __id = FIELD_GET(PCI_CAP_ID_MASK, __ent);  /* 低 8 位是 ID */ \
        if (__id == 0xff) break;                                    \
        if (__id == (cap)) { __found_pos = __pos; break; }          \
        __pos = FIELD_GET(PCI_CAP_LIST_NEXT_MASK, __ent);           \
    }                                                               \
    __found_pos;                                                    \
})
```

### 2.2 Standard Capability ID 表

内核支持的 Standard Capability ID 定义在 `pci_regs.h:219-239`：

```c
#define PCI_CAP_ID_PM       0x01    /* 电源管理 */
#define PCI_CAP_ID_MSI      0x05    /* MSI 中断 */
#define PCI_CAP_ID_PCIX     0x07    /* PCI-X */
#define PCI_CAP_ID_VNDR     0x09    /* 厂商自定义 */
#define PCI_CAP_ID_SHPC     0x0C    /* 标准热插拔控制器 */
#define PCI_CAP_ID_EXP      0x10    /* ★ PCI Express (最重要的一个) */
#define PCI_CAP_ID_MSIX     0x11    /* MSI-X 中断 */
#define PCI_CAP_ID_EA       0x14    /* 增强地址分配 */
```

**`PCI_CAP_ID_EXP`** (0x10) 是每个 PCIe 设备都必须有的——它指向 **PCI Express Capability Structure**，包含设备类型、链路能力、链路状态、设备控制等核心寄存器。

### 2.3 Extended Capability 链表

PCIe 扩展能力位于配置空间 0x100（256 字节）之后，同样用链表串联：

```c
// pci.h:188-217
#define PCI_FIND_NEXT_EXT_CAP(read_cfg, start, cap, prev_ptr, args...) \
({                                                                  \
    u16 __pos = (start) ?: PCI_CFG_SPACE_SIZE;  /* 起始 0x100 */    \
    int __ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;  \
    while (__ttl-- > 0) {                                           \
        read_cfg##_dword(args, __pos, &__header);                   \
        if (PCI_EXT_CAP_ID(__header) == (cap) && __pos != start) {  \
            __found_pos = __pos; break;                              \
        }                                                           \
        __pos = PCI_EXT_CAP_NEXT(__header);                         \
    }                                                               \
    __found_pos;                                                    \
})
```

Extended Capability 的查找范围是 0x100 ~ 0xFFF（4KB - 256B），每项占 4 字节（一个 DWORD），第一个 DWORD 的高 16 位是 Capability ID，低 12 位指向下一个的偏移。

---

## 3. PCI Express Capability — 核心寄存器块

`PCI_CAP_ID_EXP` (0x10) 指向的 PCIe 能力结构是所有 PCIe 设备都具备的寄存器块。它的布局定义在 `pci_regs.h:480-580`：

```
PCIe Capability 结构
地址偏移   寄存器                 内核宏                        Line
────────────────────────────────────────────────────────────────
0x00        PCI Express Capabilities   PCI_EXP_FLAGS            (line 481)
            [3:0]  = Capability Ver
            [7:4]  = ★ Device/Port Type
            [8]    = Slot Implemented
            [15]   = FLIT Mode
0x04        Device Capabilities      PCI_EXP_DEVCAP            (line 498)
            [2:0]  = Max_Payload_Size
            [7:4]  = Phantom Functions
            [8]    = Extended Tag
            [10:8] = L0s Acceptable Latency
            [13:11]= L1 Acceptable Latency
0x08        Device Control             PCI_EXP_DEVCTL           (line 512)
            [0]    = Correctable Err Reporting Enable
            [1]    = Non-Fatal Err Reporting Enable
            [2]    = Fatal Err Reporting Enable
            [3]    = Unsupported Request Reporting Enable
            [7:5]  = ★ Max_Payload_Size
            [8]    = Extended Tag Enable
0x0C        Link Capabilities         PCI_EXP_LNKCAP            (line 531)
            [3:0]  = ★ Max Link Speed (2.5/5.0/8.0/16.0/32.0/64.0 GT/s)
            [9:4]  = ★ Max Link Width
            [11:10]= ASPM Support (L0s/L1)
            [20:18]= L1 Exit Latency
0x10        Link Control              PCI_EXP_LNKCTL            (line 546)
            [0]    = Active State PM Control (ASPM L0s Enable)
            [1]    = ASPM L1 Enable
            [7]    = Extended Synch
0x12        Link Status               PCI_EXP_LNKSTA            (line 560)
            [3:0]  = ★ Negotiated Link Speed
            [9:4]  = ★ Negotiated Link Width
            [11]   = Link Training (set when training in progress)
0x1C        Root Control              PCI_EXP_RTCTL             (root port only)
```

### 3.1 Device/Port Type — 设备的拓扑角色

```c
// pci_regs.h:486-494
#define  PCI_EXP_TYPE_ENDPOINT      0x0  /* 普通端点（GPU, NIC） */
#define  PCI_EXP_TYPE_LEG_END       0x1  /* 传统端点 */
#define  PCI_EXP_TYPE_ROOT_PORT     0x4  /* ★ Root Port（CPU 直连的桥上口） */
#define  PCI_EXP_TYPE_UPSTREAM      0x5  /* Switch 上行口 */
#define  PCI_EXP_TYPE_DOWNSTREAM    0x6  /* ★ Switch 下行口 / Root Port 对端 */
#define  PCI_EXP_TYPE_PCI_BRIDGE    0x7  /* PCIe→PCI Bridge */
#define  PCI_EXP_TYPE_PCIE_BRIDGE   0x8  /* PCI→PCIe Bridge */
#define  PCI_EXP_TYPE_RC_END        0x9  /* Root Complex Integrated Endpoint */
#define  PCI_EXP_TYPE_RC_EC         0xa  /* Root Complex Event Collector */
```

这个 4-bit 字段决定了 `pci_setup_device`（probe.c:2038）中 `set_pcie_port_type` 的分支——Root Port 需要注册 AER/HP/PME 等服务驱动，Downstream Port 类似，Endpoint 则不需要。

### 3.2 链路速度和宽度

```c
// pci.h:565-583 — 链路速度解码宏
#define PCIE_LNKCAP_SLS2SPEED(lnkcap)       \
    (lnkcap_sls == PCI_EXP_LNKCAP_SLS_64_0GB ? PCIE_SPEED_64_0GT :  \
     lnkcap_sls == PCI_EXP_LNKCAP_SLS_32_0GB ? PCIE_SPEED_32_0GT :  \
     lnkcap_sls == PCI_EXP_LNKCAP_SLS_16_0GB ? PCIE_SPEED_16_0GT :  \
     lnkcap_sls == PCI_EXP_LNKCAP_SLS_8_0GB  ? PCIE_SPEED_8_0GT :   \
     lnkcap_sls == PCI_EXP_LNKCAP_SLS_5_0GB  ? PCIE_SPEED_5_0GT :   \
     lnkcap_sls == PCI_EXP_LNKCAP_SLS_2_5GB  ? PCIE_SPEED_2_5GT : 0)
```

| GT/s | 编码速率 | 实际有效带宽（x16） | 规范代 |
|------|---------|-------------------|--------|
| 2.5  | Gen1    | ~4 GB/s           | PCIe 1.x |
| 5.0  | Gen2    | ~8 GB/s           | PCIe 2.x |
| 8.0  | Gen3    | ~16 GB/s          | PCIe 3.0 |
| 16.0 | Gen4    | ~32 GB/s          | PCIe 4.0 |
| 32.0 | Gen5    | ~64 GB/s          | PCIe 5.0 |
| 64.0 | Gen6    | ~128 GB/s         | PCIe 6.0 |

Link Status 寄存器随时可读，反映**实际协商的**速度和宽度，Link Capability 是硬件能支持的最大值。

内核在 link 状态变化时更新 bus 信息（`pci.h:648-654`）：

```c
bus->cur_bus_speed = pcie_link_speed[linksta & PCI_EXP_LNKSTA_CLS];
bus->flit_mode = (linksta2 & PCI_EXP_LNKSTA2_FLIT) ? 1 : 0;
pci_info(dev, "speed %s, width x%d\n",
         pci_speed_string(lnkcap2), FIELD_GET(PCI_EXP_LNKSTA_NLW, linksta));
```

### 3.3 Max Payload Size（MPS）

MPS 控制一个 TLP 最多能携带多少字节的 payload。内核设置了多层机制来协商这个值：

```c
// pci_regs.h:513-525
#define  PCI_EXP_DEVCTL_PAYLOAD      0x00e0  // bits [7:5]
#define  PCI_EXP_DEVCTL_PAYLOAD_128B  0x0000
#define  PCI_EXP_DEVCTL_PAYLOAD_256B  0x0020
#define  PCI_EXP_DEVCTL_PAYLOAD_512B  0x0040
#define  PCI_EXP_DEVCTL_PAYLOAD_1024B 0x0060
#define  PCI_EXP_DEVCTL_PAYLOAD_2048B 0x0080
#define  PCI_EXP_DEVCTL_PAYLOAD_4096B 0x00a0
```

MPS 是关键性能参数：一个 TLP 最大 4KB payload，但实际由整条链路中最小的 MPS 决定。GPU 通常配置 256B 或 512B MPS，太大会增加延迟，太小会降低吞吐。

---

## 4. TLP — 事务层包

PCIe 是**包交换协议**，所有操作（寄存器读写、内存读写、中断消息、错误报告）都以 TLP（Transaction Layer Packet，事务层包）的形式在链路上传输。

### 4.1 TLP 格式字段（内核定义在 pci.h:67-78）

```c
// pci.h:67-78 — 内核中的 TLP 格式/类型常量
#define PCIE_TLP_FMT_3DW_NO_DATA   0x00  /* 3DW header, no data */
#define PCIE_TLP_FMT_4DW_NO_DATA   0x01  /* 4DW header, no data */
#define PCIE_TLP_FMT_3DW_DATA      0x02  /* 3DW header, with data */
#define PCIE_TLP_FMT_4DW_DATA      0x03  /* 4DW header, with data */

#define PCIE_TLP_TYPE_CFG0_RD      0x04  /* Config Type 0 Read */
#define PCIE_TLP_TYPE_CFG1_RD      0x05  /* Config Type 1 Read */
// 4DW: 用于 64 位地址（地址占 2 DW 而非 1 DW）
```

TLP 通用结构：
```
┌────┬──────┬──────┬──────┬───────┬───────────────┐
│Fmt │ Type │ R    │ TC   │ Attr  │    Length     │  ← Header (3-4 DW)
│2b  │ 5b   │ 1b   │ 3b   │ 3b    │    10b       │
├────┴──────┴──────┼──────┴───────┴───────────────┤
│  Requester ID    │  Tag       │  Last/First DW  │
│  (Bus:Dev:Func)  │  (8b)      │  Byte Enables   │
├──────────────────┴────────────┴────────────────┤
│  Address (32-bit or 64-bit)                    │
│  (Config: Bus:Dev:Func + 扩展 Reg#;  Memory: 字面地址)   │
├───────────────────────────────────────────────┤
│  [Data Payload: 0 ~ 4096 bytes]               │
└───────────────────────────────────────────────┘
```

### 4.2 TLP 类型速查

| TLP 类型 | 含义 | 内核宏 (pci.h) | 典型读取场景 |
|---------|------|---------------|-------------|
| CfgRd0 / CfgWr0 | Config Type 0 读写 | `PCIE_TLP_TYPE_CFG0_RD/WR = 0x04` | 访问 Endpoint 配置空间 |
| CfgRd1 / CfgWr1 | Config Type 1 读写 | `PCIE_TLP_TYPE_CFG1_RD/WR = 0x05` | Bridge 转发给下行的配置请求 |
| MRd / MWr | Memory Read/Write | — | DMA、BAR MMIO 访问 |
| Msg / MsgD | 消息 | — | INTx、PME、Error、Hotplug 等带内信号 |
| Cpl / CplD | Completion | — | Non-Posted 请求的响应 |
| CplLk / CplDLk | 锁定完成 | — | 原子操作 |
| FetchAdd / Swap / CAS | 原子操作 | — | GPU 原子操作 |

### 4.3 TLP 路由

```
Config Request:
  Type 0: 直接发给 Endpoint（不跨 Bridge）
  Type 1: Bridge 先收，转为 Type 0 发给下游设备

Memory Request:
  由地址路由（Address Routing）
  Bridge 检查地址是否在自己的 MMIO window 内
  是→转发给下游，否→忽略

Message:
  三种路由方式（pci.h:80-85）
  PCIE_MSG_TYPE_R_RC    (0) — 发给 Root Complex
  PCIE_MSG_TYPE_R_ADDR  (1) — 地址路由
  PCIE_MSG_TYPE_R_ID    (2) — ID 路由（Bus:Dev:Func）
  PCIE_MSG_TYPE_R_BC    (3) — 广播
  PCIE_MSG_TYPE_R_LOCAL (4) — 本地（不跨链路）
  PCIE_MSG_TYPE_R_GATHER (5) — 收集路由
```

### 4.4 几个重要 TLP 实例

**MSG 消息类型**（内核中用于 INTx 模拟和 PME）：

```c
// pci.h:88 — PME Turn Off
#define PCIE_MSG_CODE_PME_TURN_OFF    0x19

// pci.h:91-98 — INTx 模拟（传统中断）
#define PCIE_MSG_CODE_ASSERT_INTA     0x20
#define PCIE_MSG_CODE_ASSERT_INTB     0x21
#define PCIE_MSG_CODE_ASSERT_INTC     0x22
#define PCIE_MSG_CODE_ASSERT_INTD     0x23
#define PCIE_MSG_CODE_DEASSERT_INTA   0x24
#define PCIE_MSG_CODE_DEASSERT_INTB   0x25
#define PCIE_MSG_CODE_DEASSERT_INTC   0x26
#define PCIE_MSG_CODE_DEASSERT_INTD   0x27
```

**Completion Status code**（在 Completion TLP 中表示请求是否成功）：

```c
// pci.h:101
#define PCIE_CPL_STS_SUCCESS  0x00  /* Successful Completion */
```

常见的 Completion 状态码：
| Status | 含义 | 对应场景 |
|--------|------|---------|
| 0x00 | Successful Completion | 正常 |
| 0x01 | Unsupported Request (UR) | 设备不认识这个请求 |
| 0x02 | Configuration Request Retry Status (CRS) | 设备还没准备好，重试 |
| 0x04 | Completer Abort (CA) | 设备收到了但因内部错误拒绝 |

UR 和 CA 是 AER 错误报告的常见来源——设备收到越界的 BAR 访问或无效的配置空间读写都会返回这些状态。

---

## 5. MSI/MSI-X 中断的协议层

MSI 和 MSI-X 都是**带内中断**——不是走物理 INTx 线，而是设备向一个指定的内存地址写一个数据值（Memory Write TLP）。CPU/APIC 收到这个 memory write 后触发中断。

```c
// pci_regs.h:315-329 — MSI capability 寄存器布局
#define PCI_MSI_FLAGS       0x02    /* Message Control */
#define  PCI_MSI_FLAGS_ENABLE     0x0001  /* 使能 */
#define  PCI_MSI_FLAGS_64BIT      0x0080  /* 支持 64-bit 地址 */
#define  PCI_MSI_FLAGS_MASKBIT    0x0100  /* 支持 per-vector masking */
#define PCI_MSI_ADDRESS_LO  0x04    /* Message Address (低32位) */
#define PCI_MSI_ADDRESS_HI  0x08    /* Message Address (高32位, 64-bit only) */
#define PCI_MSI_DATA_32     0x08    /* Message Data (32-bit address mode) */
#define PCI_MSI_DATA_64     0x0C    /* Message Data (64-bit address mode) */
#define PCI_MSI_MASK_32     0x0C    /* Mask Bits (per-vector, optional) */
#define PCI_MSI_PENDING_32  0x10    /* Pending Bits */
```

MSI 产生中断的核心机制：
```
设备写 PCI_MSI_ADDRESS（系统物理地址）← 这是 CPU APIC 的 memory-mapped 地址
      写入 PCI_MSI_DATA（特定的中断向量号）
      → APIC 识别这个 memory write 为中断
        → 转发给目标 CPU core
```

```c
// pci_regs.h:332-341 — MSI-X capability 寄存器布局
#define PCI_MSIX_FLAGS      2       /* Message Control */
#define  PCI_MSIX_FLAGS_QSIZE     0x07FF  /* Table size (N-1) */
#define  PCI_MSIX_FLAGS_MASKALL   0x4000  /* Mask All */
#define  PCI_MSIX_FLAGS_ENABLE    0x8000  /* Enable */
#define PCI_MSIX_TABLE      4       /* MSI-X Table 偏移 */
#define  PCI_MSIX_TABLE_BIR       0x00000007  /* 用哪个 BAR */
#define  PCI_MSIX_TABLE_OFFSET    0xfffffff8  /* BAR 内偏移 */
#define PCI_MSIX_PBA        8       /* Pending Bit Array 偏移 */
```

MSI-X 相比 MSI 的关键区别：

| | MSI | MSI-X |
|---|-----|-------|
| 向量数 | 1/2/4/8/16/32 | 最多 2048 |
| 数据存放 | 在 capability 寄存器内 | 在 BAR 中的 MSI-X Table（MMIO） |
| per-vector mask | 可选（PCI_MSI_FLAGS_MASKBIT） | 每个 entry 都有 Mask 位 |
| per-vector 地址 | 所有向量共用一个地址 | 每个向量可以独立指定地址 |
| Multi-Vector 中断 | 用不同的 Message Data 区分 | 和 MSI 一样的方法，最多 2048 |

GPU 和 RDMA 网卡用 MSI-X 的原因：数千个 Queue 各需独立中断向量，per-vector 的 affinity 让 NUMA 节点就近处理中断。

---

## 6. AER 错误的协议层

AER 依赖 PCI Express Extended Capability（扩展能力空间）。`pci_aer_init()`（probe.c:2674）在设备枚举期间通过 `PCI_FIND_NEXT_EXT_CAP` 遍历查找 AER 能力。

AER 寄存器块 (在扩展能力空间中的偏移):

```
Register             内核宏                         含义
────────────────────────────────────────────────────────
PCI_ERR_UNCOR_STATUS   N/A                          未纠正错误状态
  bits[0]:   Undefined (TLP 损坏)
  bits[4]:   Data Link Protocol Error
  bits[12]:  Poisoned TLP Received
  bits[13]:  Flow Control Protocol Error
  bits[14]:  Completion Timeout
  bits[15]:  Completer Abort
  bits[16]:  Unexpected Completion
  bits[17]:  Receiver Overflow
  bits[18]:  Malformed TLP
  bits[19]:  ECRC Error
  bits[20]:  Unsupported Request Error
PCI_ERR_UNCOR_MASK     N/A                          未纠正错误掩码
PCI_ERR_UNCOR_SEVERITY N/A                          未纠正错误的严重度（Fatal vs Non-Fatal）
PCI_ERR_COR_STATUS     N/A                          可纠正错误状态
  bits[0]:   Receiver Error
  bits[6]:   Bad TLP
  bits[7]:   Bad DLLP
  bits[8]:   REPLAY_NUM Rollover
  bits[12]:  Replay Timer Timeout
  bits[13]:  Advisory Non-Fatal Error
PCI_ERR_COR_MASK       N/A                          可纠正错误掩码
PCI_ERR_CAP             N/A                          AER 能力：
  bits[4:0]: First Error Pointer
  bits[6]:   ECRC Check Capable
  bits[7]:   ECRC Check Enable
  bits[8]:   ECRC Generation Capable
  bits[9]:   ECRC Generation Enable
  bits[10]:  Multiple Header Recording Capable
  bits[11]:  Multiple Header Recording Enable
PCI_ERR_HEADER_LOG     N/A                          TLP Header Log (多个DWord)
PCI_ERR_ROOT_COMMAND    N/A                          Root Complex Error Command
PCI_ERR_ROOT_STATUS     N/A                          Root Complex Error Status
PCI_ERR_ROOT_COR_SRC    N/A                          Root Correctable Error Source ID
PCI_ERR_ROOT_SRC        N/A                          Root Error Source ID

Root Port 控制/状态 (aer.c 通过 aer_irq 读取):
PCI_ERR_ROOT_ERR_SRC → 告诉 aer_irq 是哪个 endpoint 报的错
PCI_ERR_ROOT_STATUS   → 报告发生了哪种类型的错误
  - PCI_ERR_ROOT_COR_RCV  (bit 0)  收到 Correctable Error
  - PCI_ERR_ROOT_MULT_COR_RCV  (bit 1)  收到多个 Correctable
  - PCI_ERR_ROOT_UNCOR_RCV (bit 2)  收到 Uncorrectable Error
  - PCI_ERR_ROOT_MULT_UNCOR_RCV (bit 3)  收到多个 Uncorrectable
  - PCI_ERR_ROOT_FIRST_FATAL  (bit 4)  第一个错误是 Fatal
  - PCI_ERR_ROOT_NONFATAL_RCV (bit 5)  收到 Non-Fatal Error
```

内核 AER 驱动的处理入口（aer.c:1470）：

```c
static irqreturn_t aer_irq(int irq, void *context)
{
    struct aer_rpc *rpc = context;
    struct aer_err_source e_src;

    // 读 Root Port 寄存器获取错误源
    pci_read_config_dword(rpc->rpd, aer + PCI_ERR_ROOT_STATUS, &e_src.status);
    pci_read_config_dword(rpc->rpd, aer + PCI_ERR_ROOT_ERR_SRC, &e_src.id);

    kfifo_put(&rpc->aer_fifo, e_src);   // 交给 threaded handler
    return IRQ_WAKE_THREAD;
}
```

---

## 7. Power Management 协议层

### 7.1 设备电源状态

| 状态 | 恢复延迟 | 功耗 | 可访问配置空间 |
|------|---------|------|--------------|
| D0 | 0（活跃） | 100% | ✓ |
| D1 | 低 | ~20% | ✓ |
| D2 | 更低 | ~5% | ✓ |
| D3hot | 高（10ms~） | <1% | ✓ |
| D3cold | 非常高（100ms~） | 0（断电） | ✗（恢复后可见） |

从 D3hot 恢复至少等 10ms（`pci.h:259`）：

```c
#define PCI_PM_D3HOT_WAIT   10   /* 10 ms */
```

### 7.2 PCI Express Link PM

PCIe 链路本身也有独立的电源状态（`pci_regs.h:531` 的 LnkCap 位 [11:10]）：

| 状态 | 退出延迟 | 含义 |
|------|---------|------|
| L0 | 0 | 链路全速 |
| L0s | ~µs | 单向 standby（一端空闲，可快速恢复） |
| L1 | ~µs-100µs | 双向 idle（两端都空闲了下 PLL） |
| L1.1/L1.2 | 50-100µs | L1 Substates，更深睡眠 |

内核 ASPM（Active State Power Management）代码在 `aspm.c` 中通过写 Link Control 寄存器的 `ASPM L0s Enable / ASPM L1 Enable` 位实现，结构体为 `struct pcie_link_state`（aspm.c:228-248）。

---

## 8. PCIe 拓扑全景图

PCIe 拓扑中只有**三种角色**：

| 角色 | 内核 Type | 做什么 | 典型实体 |
|------|----------|--------|---------|
| **Root Complex** | — | CPU 侧的 PCIe 入口，把 CPU 的内存/IO 请求翻译成 TLP 包 | 集成在 CPU die 内 |
| **Switch** | Type 5(US)+Type 6(DS) | 数据包"分路器"，上行口收→根据地址路由到下行口 | PCIe Switch 芯片 |
| **Endpoint** | Type 0 | 叶节点，不转发，只产生/消费数据 | GPU、NVMe、NIC |

### 8.1 三种角色与 7 种 Type

PCIe 拓扑中只有三种角色，但寄存器里用 **7 种 Type 码** 区分：

| Type | 寄存器值 | 角色 | 说明 |
|------|---------|------|------|
| **Type 0** | 0x0 | Endpoint | 普通设备：GPU、NVMe、NIC |
| **Type 1** | 0x1 | Endpoint | Legacy Endpoint（PCI 时代遗留） |
| **Type 4** | 0x4 | Root Complex | Root Port：CPU 直连的 PCIe 出口 |
| **Type 5** | 0x5 | Switch | Switch 上行口（Upstream Port），连向 Root Port |
| **Type 6** | 0x6 | Switch | Switch 下行口（Downstream Port），连向 Endpoint |
| **Type 9** | 0x9 | Root Complex | RC Integrated Endpoint，集成在 CPU 内部的设备 |
| **Type A** | 0xA | Root Complex | RC Event Collector，收集 RCiEP 的错误事件 |

```
内核宏 (pci_regs.h:486-494):
  PCI_EXP_TYPE_ENDPOINT    = 0x0    PCI_EXP_TYPE_LEG_END  = 0x1
  PCI_EXP_TYPE_ROOT_PORT   = 0x4    PCI_EXP_TYPE_UPSTREAM = 0x5
  PCI_EXP_TYPE_DOWNSTREAM  = 0x6    PCI_EXP_TYPE_RC_END   = 0x9
  PCI_EXP_TYPE_RC_EC       = 0xA
```

### 8.2 拓扑全景图

```
                           ╔══════════════════════════════╗
                           ║      Root Complex (RC)       ║
                           ║  集成在 CPU die 内，拓扑根    ║
                           ║                              ║
                           ║  ┌──────────────────────┐    ║
                           ║  │  Root Port 1 (Type 4)│    ║    ← CPU 直连出口
                           ║  │  Root Port 2 (Type 4)│    ║
                           ║  │  RCiEP      (Type 9) │    ║    ← 集成 GPU/NIC
                           ║  │  RCEC       (Type A) │    ║    ← 错误收集
                           ║  └──────────┬───────────┘    ║
                           ╚══════════════╪════════════════╝
                                          │ PCIe Link (x16, Gen5 32GT/s)
                                          │
         ╔════════════════════════════════╪═══════════════════════════╗
         ║                         PCIe Switch                        ║
         ║                     数据包分路器                           ║
         ║                                                           ║
         ║  ┌─────────────────────────────────────────────────────┐  ║
         ║  │            Upstream Port (Type 5)                   │  ║ ← 连向 Root Port
         ║  │            收上游 TLP，查地址表路由                    │  ║
         ║  └──────────────────────┬──────────────────────────────┘  ║
         ║                         │ Internal Virtual Bus            ║
         ║            ┌────────────┼────────────┐                    ║
         ║  ┌─────────▼──────┐ ┌───▼──────────┐ │                    ║
         ║  │ Downstream Port│ │Downstream Port│ │ ← Type 6           ║
         ║  │   (Type 6)     │ │   (Type 6)    │ │   连向 Endpoint    ║
         ║  │   链路 x8      │ │   链路 x4      │ │                    ║
         ║  └───────┬────────┘ └───────┬────────┘ │                    ║
         ╚══════════╪══════════════════╪═══════════╪══════════════════╝
                    │                  │           │
                    │                  │           │ 直连 EP (无 Switch)
    ╔═══════════════╪══════╗ ╔════════╪══════╗ ╔══╪══════════════════╗
    ║    GPU               ║ ║  NVMe         ║ ║  NIC               ║
    ║  Endpoint (Type 0)   ║ ║ Endpoint(T0)  ║ ║ Endpoint (Type 0) ║
    ║  10DE:2684           ║ ║  144D:A80A    ║ ║  直连 Root Port   ║
    ║  BAR0: MMIO 256MB    ║ ║  BAR0: 16KB   ║ ║  BAR0: MMIO       ║
    ║  BAR3: VRAM 32MB+    ║ ║               ║ ║                   ║
    ╚══════════════════════╝ ╚═══════════════╝ ╚═══════════════════╝
```

### 8.3 CPU 如何一步步发现 GPU

```
加电 (T=0)
 │
 ├── 硬件复位阶段 (~100ms)
 │   REFCLK 稳定 (100µs)  →  释放 PERST#  →  链路训练
 │   协商 Gen 速度 (2.5~64GT/s) + Lane 宽度 (x1~x16)
 │   Link Up!  Root Port LNKSTA 寄存器反映实际协商结果
 │
 ├── 配置空间就绪 (~100ms)
 │   端点可能返回 Configuration Retry Status (CRS) 表示"还没准备好"
 │   Root Complex 硬件自动重试，直到端点返回 Successful Completion
 │   此时 Vendor/Device ID/Class Code/BAR 等寄存器可读
 │
 ├── 内核枚举阶段
 │   pci_scan_child_bus(bus0)                              ← probe.c:3210
 │     └→ pci_scan_device(bus, devfn)                      ← probe.c:2602
 │         读 Vendor:Device → 分配 pci_dev
 │         pci_setup_device → hdr_type/class/BAR/Cap        ← probe.c:2021
 │         pci_init_capabilities (24 种能力初始化)           ← probe.c:2655
 │       └→ 如果是 Bridge (hdr_type=1) → 递归扫描下游 bus
 │
 ├── 驱动匹配阶段
 │   __pci_register_driver(amdgpu/nouveau)                 ← probe.c:1471
 │     └→ pci_match_device: 对表 [Vendor:10DE, Device:2684] ← probe.c:136
 │       命中 → pci_device_probe                            ← probe.c:473
 │         └→ drv->probe(dev, id)  ← GPU 驱动初始化
 │             注册 DRM device, 建立 GPU 页表, 分配 IRQ
 │
 └── 用户态可用
     /dev/dri/renderD128  用户可以 open/ioctl/mmap
```

**Switch 路径的关键**：每经过一层 Switch，bus 号 +1。内核通过 `pci_upstream_bridge(dev)`（`pci.h:819`）在 `dev->bus->self` 中追溯上游 bridge，`pcie_find_root_port(dev)`（`pci.h:2651`）一路爬到 Root Port。Switch 的 Upstream Port (Type 5) 只注册 PME 服务；Downstream Port (Type 6) 额外承载 AER、Hotplug、DPC、Bandwidth Control。

**CPU 如何一步步发现 GPU**：加电 → 链路训练（~100ms，协商 Gen 速度和 Lane 宽度）→ Root Port 检测到设备在位 → 内核枚举：读配置空间 Vendor/Device ID、分配 BAR 地址空间、初始化 Capability 链表 → 驱动匹配：`pci_match_device` 对表 `[10DE:2684]` → 驱动 `probe()` 初始化 GPU → 用户态可用。

**从 EP 视角**：每层 Switch 增加一级 bus 号（`bus->parent` 追溯），内核通过 `pci_upstream_bridge` 和 `pcie_find_root_port` 在 `pci_dev` 树中向上或向下导航。Switch Upstream Port 只注册 PME 服务，Downstream/Root Port 额外注册 AER、Hotplug、DPC。

---

## 9. 与系列文章的关系

本文作为**第 0 篇**，提供了阅读后续 7 篇文章的协议层知识基础：

| 后续文章 | 依赖的协议知识 |
|---------|--------------|
| **01-枚举** | 配置空间、ECAM、HdrType、Class Code |
| **02-BAR** | BAR 寄存器位域、64-bit BAR、prefetchable、ReBAR |
| **03-驱动probe** | Vendor/Device ID、Class Code、capability 链表 |
| **04-MSI-X** | MSI/MSI-X 寄存器布局、Message Address/Data |
| **05-AER** | AER Extended Capability、Route Complex 错误寄存器 |
| **06-热插拔/PM** | Slot Control、D0-D3、ASPM L0s/L1/L1SS |
| **07-Port服务** | Device/Port Type 决定服务分发 |

## 下一篇文章

→ [第1篇：枚举 — 内核怎么发现 PCIe 设备](./01-enumeration.md) — 本文讲了内核**读什么**寄存器，下一篇讲内核**什么时候读**、**按什么顺序读**。