---
title: '修复 8250 串口驱动共享 IRQ 竞态导致的 use-after-free'
date: '2026/05/29 13:32:19'
updated: '2026/05/29 17:01:27'
---

# 修复 8250 串口驱动共享 IRQ 竞态导致的 use-after-free

## 背景

Linux 8250 串口驱动在多个串口共享同一个 IRQ 中断线时，使用一个以 IRQ 号为 key 的哈希表 `irq_lists` 来管理共享同一个 IRQ 的所有端口，每个 IRQ 对应一个 `irq_info` 结构体，端口通过 `list` 成员链接到 `irq_info->head` 指向的链表上。

| 结构 | 作用 |
|------|------|
| `irq_lists` | 全局哈希表，以 IRQ 号为 key |
| `irq_info` | 每个 IRQ 一条记录，包含 `head`（端口链表头）、`lock`（自旋锁）|
| `hash_mutex` | 全局互斥锁，保护哈希表查找/插入/删除 |
| `i->lock` | 每个 IRQ 的自旋锁，保护端口链表操作 |

关键函数：
- **`serial_get_or_create_irq_info()`** — 根据 IRQ 号查找或创建 `irq_info`
- **`serial_link_irq_chain()`** — 将端口加入 IRQ 共享链表，首个端口调用 `request_irq()` 注册中断处理函数
- **`serial_unlink_irq_chain()`** — 将端口从链表移除，链表中最后一个端口被移除时调用 `free_irq()` 并 `kfree(irq_info)`

## 问题分析

### 现场现象

Bugzilla [221579](https://bugzilla.kernel.org/show_bug.cgi?id=221579) 报告者 Wang Zhaolong 提供了一个竞态复现脚本：多个进程同时对 `/dev/ttyS1` 和 `/dev/ttyS3`（共享 IRQ 3）进行高频 `open/close` 操作，触发如下警告：

```
[   61.937561][  T915] ------------[ cut here ]------------
[   61.938276][  T915] Unbalanced enable for IRQ 3
[   61.938828][  T915] WARNING: kernel/irq/manage.c:774 at __enable_irq+0x33/0x60
[   61.940190][  T915] Call Trace:
[   61.940190][  T915]  enable_irq+0x7e/0x100
[   61.940190][  T915]  serial8250_do_startup+0x7ce/0xa80
[   61.940190][  T915]  uart_port_startup+0x13d/0x440
[   61.940190][  T915]  uart_port_activate+0x5b/0xb0
[   61.940190][  T915]  tty_port_open+0x90/0x110
[   61.940190][  T915]  uart_open+0x1e/0x30
```

### 根因分析

问题涉及两个并发路径之间的竞态：

#### 路径 1：link 路径（端口 open）

`serial_link_irq_chain()` 被 `serial8250_startup()` → `uart_port_startup()` 调用：

```c
static int serial_link_irq_chain(struct uart_8250_port *up)
{
    struct irq_info *i;
    int ret;

    i = serial_get_or_create_irq_info(up);  // 查找/创建 irq_info
    if (IS_ERR(i))
        return PTR_ERR(i);

    spin_lock_irq(&i->lock);
    if (i->head) {
        // 已有其他端口注册了该 IRQ，直接加入链表
        list_add(&up->list, i->head);
        spin_unlock_irq(&i->lock);
        return 0;
    }

    // 这是第一个使用该 IRQ 的端口
    INIT_LIST_HEAD(&up->list);
    i->head = &up->list;
    spin_unlock_irq(&i->lock);
    // hash_mutex 在此处已经被释放
    ret = request_irq(up->port.irq, ...);   // 注册中断处理函数
    ...
}
```

#### 路径 2：unlink 路径（端口 close）

`serial_unlink_irq_chain()` 被 `serial8250_shutdown()` → `uart_port_shutdown()` 调用：

```c
static void serial_unlink_irq_chain(struct uart_8250_port *up)
{
    struct irq_info *i;

    mutex_lock(&hash_mutex);
    hash_for_each_possible(irq_lists, i, node, up->port.irq) {
        if (i->irq == up->port.irq) {
            if (list_empty(i->head))
                free_irq(up->port.irq, i);  // 释放 IRQ + kfree(i)
            serial_do_unlink(i, up);        // 从链表移除端口
            return;
        }
    }
    mutex_unlock(&hash_mutex);
}
```

#### 竞态窗口

```
CPU0 (Port A open, 首次)                 CPU1 (Port A close, 最后一个端口)
──────────────────────────────────────   ─────────────────────────────────────
serial_link_irq_chain()
  i = get_or_create()  // 新分配
  i->head = &portA->list // 发布 i->head
  spin_unlock_irq(&i->lock)
                                         serial_unlink_irq_chain()
                                           mutex_lock(&hash_mutex)
                                           // 找到 i
                                           list_empty(i->head) → TRUE
                                             free_irq()        // 释放在建的 IRQ！
                                             kfree(i)          // 释放 i！
  request_irq(..., i)  // ← i 已被 free！use-after-free!
```

**备注**：本次修复之前的原始代码中，`hash_mutex` 在 `serial_get_or_create_irq_info()` 内部通过 `guard(mutex)(&hash_mutex)` 持有，函数返回时自动释放，比 `i->head` 发布时间更早，并发 unlink 可直接在哈希表中找到 `i` 并 free 它。v1 已将锁提升到 `serial_link_irq_chain()` 入口。

#### 为什么表现为 "Unbalanced enable for IRQ"？

1. `free_irq()` → `__free_irq()` → `irq_shutdown()` 将 `desc->depth` 递增
2. 同时第一个端口通过 `serial8250_do_startup()` → `serial8250_THRE_test()` 执行 `disable_irq_nosync()` / `enable_irq()` 配对
3. 由于 `desc->depth` 被 `irq_shutdown()` 递增到 1，`enable_irq()` 调用 `__enable_irq()` 时发现 `depth == 1`，本应是 0 才能正常 enable，触发 `Unbalanced enable for IRQ` 警告

`kernel/irq/manage.c:774`：
```c
void __enable_irq(struct irq_desc *desc)
{
    switch (desc->depth) {
    case 0:
        // 正常 disable_irq/enable_irq 配对路径
 err_out:
        WARN(1, KERN_WARNING "Unbalanced enable for IRQ %d\n",
             irq_desc_get_irq(desc));
        break;
    case 1: {
        // irq_shutdown() 设置的 depth=1，走到这里
        ...
    }
    }
}
```

## 修复方案

### v1

v1 是原始补丁，将 `hash_mutex` 的加锁从 `serial_get_or_create_irq_info()`（`guard(mutex)` 自动在函数返回时释放）提升到 `serial_link_irq_chain()` 入口，使其覆盖 `i->head` 检查和 `list_add/INIT_LIST_HEAD` 操作，防止并发 unlink 在此期间找到 `i` 并 free 它。

```c
// v1: hash_mutex 覆盖 i->head 检查和链表操作
mutex_lock(&hash_mutex);
i = serial_get_or_create_irq_info(up);
// ...
INIT_LIST_HEAD(&up->list);
i->head = &up->list;
spin_unlock_irq(&i->lock);

mutex_unlock(&hash_mutex);  // ← request_irq() 之前释放

ret = request_irq(...);
```

### v1 → v2

代码无变化，仅添加 `Reported-by: Wang Zhaolong <wangzhaolong@fnnas.com>` tag。

### v2 → v3（IRQ 初始化竞态）

v2 虽然解决了 link/unlink 之间的 use-after-free，但 Wang Zhaolong 实测仍然报告 `Unbalanced enable for IRQ`。

根因在于 v2 在 `request_irq()` 前释放了 `hash_mutex`：

```
CPU0 (Port A open, 首个端口)             CPU1 (Port B open, 共享同一个 IRQ)
──────────────────────────────────────   ─────────────────────────────────────
serial_link_irq_chain()
  mutex_lock(&hash_mutex)
  i = get_or_create()
  INIT_LIST_HEAD(&up->list)
  i->head = &portA->list   // 发布!
  spin_unlock_irq(&i->lock)
  mutex_unlock(&hash_mutex)  // ← 释放!
                                         serial_link_irq_chain()
                                           mutex_lock(&hash_mutex)
                                           i = get_or_create() // 返回 i（已有）
                                           i->head != NULL → list_add()
                                           mutex_unlock(&hash_mutex)
                                           return 0;
                                         // Port B 认为 IRQ 已就绪
                                         serial8250_do_startup()
                                           disable_irq_nosync()
                                           ...
                                           enable_irq()    // ← IRQ 还没建好！
                                           → WARN: Unbalanced enable

  request_irq(...) ← 此时 IRQ 才真正注册
```

**v3 修复**：将 `mutex_unlock(&hash_mutex)` 移到 `request_irq()` **返回之后**，确保在首个 IRQ 完全初始化之前，其他共享端口的 link 操作被阻塞：

```c
// v3: hash_mutex 覆盖到首个 request_irq() 完成
INIT_LIST_HEAD(&up->list);
i->head = &up->list;
spin_unlock_irq(&i->lock);

ret = request_irq(up->port.irq, serial8250_interrupt,
                  up->port.irqflags, up->port.name, i);

mutex_unlock(&hash_mutex);  // ← request_irq() 之后才释放
```

### v3 → v4（错误路径竞态 + changelog 措辞修正）

v3 修复了正常路径的竞态，但 Wang Zhaolong 指出 `request_irq()` **失败时的错误路径**仍然有竞态窗口：

```c
// v3: 错误路径 — hash_mutex 在清理前释放
ret = request_irq(...);
mutex_unlock(&hash_mutex);     // ← 先放锁
if (ret < 0)
    serial_do_unlink(i, up);  // ← 再清理（无保护！）
```

此时 `i` 仍在 `irq_lists` 中，`i->head` 已发布，另一个端口可能在 `serial_do_unlink()` 完成前 join 进来，看到非空的 `i->head` 就返回成功——但实际没有安装 IRQ handler。

**v4 修复**：错误路径中 `serial_do_unlink()` 必须在 `mutex_unlock()` **之前**执行：

```c
// v4: 清理在 hash_mutex 保护下完成
ret = request_irq(...);
if (ret < 0) {
    serial_do_unlink(i, up);  // ← 先清理（锁保护）
    mutex_unlock(&hash_mutex);
    return ret;
}
mutex_unlock(&hash_mutex);
```

同时修正了 commit message 中关于 `irq_shutdown()` 的不准确描述：原为 "hard-sets desc->depth to 1"，改为 "increments desc->depth"。

### `hash_mutex` 跨过 `request_irq()` 不会死锁

`request_irq()` 内部锁链为：

```
request_irq()
  → request_threaded_irq()
    → __setup_irq()
      → mutex_lock(&desc->request_mutex)
      → chip_bus_lock(desc)
      → raw_spin_lock_irqsave(&desc->lock)
```

link 路径：`hash_mutex` → `desc->request_mutex` → `desc->lock`
unlink 路径（已有代码）：`hash_mutex` → `desc->request_mutex` → `desc->lock`

两路径锁顺序一致，无 ABBA 死锁风险。

## 补丁链接

- v1: https://lore.kernel.org/r/20260528-bug-221579-8250-shared-irq-race-v1-1-30980cca02f3@gmail.com
- v2: https://lore.kernel.org/r/20260528-bug-221579-8250-shared-irq-race-v2-1-06531202e54d@gmail.com
- v3: https://lore.kernel.org/r/20260529-bug-221579-8250-shared-irq-race-v3-1-fe4d430862a9@gmail.com
- v4: https://lore.kernel.org/r/20260529-bug-221579-8250-shared-irq-race-v4-1-cfda63b4420f@gmail.com

Fixes: 768aec0b5bcc ("serial: 8250: fix shared interrupts issues with SMP and RT kernels")
Closes: https://bugzilla.kernel.org/show_bug.cgi?id=221579
Reported-by: Wang Zhaolong <wangzhaolong@fnnas.com>
