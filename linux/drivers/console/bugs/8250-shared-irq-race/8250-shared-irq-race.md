# 8250 共享中断竞态：use-after-free 与 "Unbalanced enable for IRQ"

## Bug 概述

- **Bugzilla**: [#221579](https://bugzilla.kernel.org/show_bug.cgi?id=221579)
- **报告者**: Wang Zhaolong <wangzhaolong@fnnas.com>
- **Fixes commit**: `768aec0b5bcc` ("serial: 8250: fix shared interrupts issues with SMP and RT kernels")
- **涉及文件**: `drivers/tty/serial/8250/8250_core.c`
- **症状**: irq_info 结构体 use-after-free，内核 "Unbalanced enable for IRQ" 警告

## 背景

8250 串口驱动维护一个以 IRQ 号为键的哈希表 `irq_lists`，每个表项是一个 `struct irq_info`。
当多个 8250 串口共享同一条 IRQ 线时，它们被链接到同一个 `irq_info` 下的链表（`i->head`）中。
第一个加入链表的端口调用 `request_irq()` 注册中断处理函数，后续端口仅将自己加入链表。

两把锁保护这个结构：

| 锁            | 作用域                   |
|--------------|--------------------------|
| `hash_mutex` | 哈希表查找/修改            |
| `i->lock`    | 每个 irq_info 的链表操作    |

## 根因分析：竞态窗口

### 修复前的代码

```c
static int serial_link_irq_chain(struct uart_8250_port *up)
{
    struct irq_info *i;
    int ret;

    i = serial_get_or_create_irq_info(up);  // 内部获取并释放 hash_mutex
    if (IS_ERR(i))
        return PTR_ERR(i);
    // <--- hash_mutex 已在此处释放

    scoped_guard(spinlock_irq, &i->lock) {  // <--- 竞态窗口
        if (i->head) {
            list_add(&up->list, i->head);
            return 0;
        }
        INIT_LIST_HEAD(&up->list);
        i->head = &up->list;
    }

    ret = request_irq(up->port.irq, serial8250_interrupt, ...);
    // ... 错误处理
}
```

`serial_get_or_create_irq_info()` 内部使用 `guard(mutex)(&hash_mutex)`，函数返回时自动释放锁。
这导致从 `serial_get_or_create_irq_info()` 返回到获取 `i->lock` 之间存在一个竞态窗口。

### 竞态 1：use-after-free（并发 unlink）

```
CPU0: link_irq_chain()            CPU1: unlink_irq_chain()
  i = get_or_create()              // hash_mutex 已释放
                                    mutex_lock(&hash_mutex)
                                    // 找到 i->head（链表非空）
                                    // 但稍后...
  // <--- 竞态：i->lock 尚未获取
                                    // 最后一个端口脱离链表
                                    list_del(&up->list)
                                    // i->head == NULL
                                    free_irq(...)
                                    kfree(i)  // i 被释放！
  spin_lock(&i->lock)  // USE-AFTER-FREE
```

`serial_unlink_irq_chain()` 全程持有 `hash_mutex`。当正在加入的端口尚未将 `i->head` 发布到链表、
但 `irq_info` 已经存在于哈希表中且没有其他端口时，unlink 侧会观察到 `list_empty(i->head)` 为真，
从而释放 `i` 并调用 `kfree(i)`，此时 link 侧正试图获取 `i->lock`，触发 use-after-free。

### 竞态 2："Unbalanced enable for IRQ" 警告

```
CPU0: link_irq_chain()              CPU1: link_irq_chain()（第二个端口，同 IRQ）
  i = get_or_create()
  // hash_mutex 已释放
  scoped_guard(i->lock):
    i->head = &up->list
  // scoped_guard 结束，i->lock 释放
  // hash_mutex 未持有
                                      mutex_lock(&hash_mutex) // 可以获取！
                                      i = get_or_create() // 找到已有的 i
                                      scoped_guard(i->lock):
                                        i->head != NULL
                                        list_add(&up->list, i->head)
                                        return 0
                                      // 端口2 已加入链，中断处理函数已注册

  // request_irq() 仍在进行中
  // CPU1 的端口2 认为中断已就绪，THRE 测试运行
  // -> disable_irq(irq) -> desc->depth++
  // 但 CPU0 上的 request_irq() 失败了！
  // -> free_irq() -> irq_shutdown() -> desc->depth++
  // 此时 desc->depth 为 2 而非 1
  // -> enable_irq() 在 THRE 测试中只能减到 1
  // -> "Unbalanced enable for IRQ" 警告
```

当 `i->head` 发布后且 `hash_mutex` 释放后（在 `scoped_guard` 结束时），另一个端口可以加入链
并在 CPU0 上的 IRQ 启动完成之前运行共享中断的 THRE 测试。如果 `request_irq()` 随后失败，
`free_irq()` 路径中的 `irq_shutdown()` 会递增 `desc->depth`，破坏 `serial8250_THRE_test()`
中的 `disable_irq`/`enable_irq` 配对。

## 修复演进

### V1 — 初始修复

将 `hash_mutex` 拉入 `serial_link_irq_chain()`，在首次 `request_irq()` 完成前一直持有。
将 `serial_get_or_create_irq_info()` 中的 `guard(mutex)(&hash_mutex)` 替换为 `lockdep_assert_held(&hash_mutex)`。

```c
static int serial_link_irq_chain(struct uart_8250_port *up)
{
    mutex_lock(&hash_mutex);
    i = serial_get_or_create_irq_info(up);
    // ... 手动加锁/解锁
    mutex_unlock(&hash_mutex);
    return ...;
}
```

- **问题**：使用了手动的 `mutex_lock/unlock` 而非 guard 风格。

### V2 — 添加 Reported-by 标签

添加 `Reported-by: Wang Zhaolong <wangzhaolong@fnnas.com>`。

### V3 — hash_mutex 跨越 request_irq() 完成

将 `hash_mutex` 的持有范围扩展到 `request_irq()` 调用之后，关闭第二个端口加入的竞态窗口。

- **问题**：`request_irq()` 失败路径未受保护 — `hash_mutex` 在 `serial_do_unlink()` 之前已释放，
  留出另一个端口在无中断处理函数的情况下加入链的窗口。

### V4 — 修复错误路径清理

在 `request_irq()` 失败时将清理操作也放在 `hash_mutex` 保护下。修正 commit message 中
`irq_shutdown()` 的描述（"hard-sets" → "increments"）。

### V5 — 添加静态分析注解

根据 Greg 的 review，为 `serial_get_or_create_irq_info()` 添加 `__must_hold(&hash_mutex)` 注解，
便于静态分析工具检测。

### V6 — 改回 guard 风格

根据 Jiri 的 review：手动加锁是不必要的。锁的生命周期（函数入口获取，每个返回路径自动释放）恰好
是 `guard(mutex)` 的语义。同样，`i->lock` 仅在调用 `request_irq()` 之前的链表操作期间持有，
适合 `scoped_guard(spinlock_irq)`。

**最终代码（v6）：**

```c
static struct irq_info *serial_get_or_create_irq_info(const struct uart_8250_port *up)
    __must_hold(&hash_mutex)
{
    struct irq_info *i;

    lockdep_assert_held(&hash_mutex);

    hash_for_each_possible(irq_lists, i, node, up->port.irq)
        if (i->irq == up->port.irq)
            return i;
    // ... 分配并返回 ...
}

static int serial_link_irq_chain(struct uart_8250_port *up)
{
    struct irq_info *i;
    int ret;

    guard(mutex)(&hash_mutex);  // 函数全程持有

    i = serial_get_or_create_irq_info(up);
    if (IS_ERR(i))
        return PTR_ERR(i);

    scoped_guard(spinlock_irq, &i->lock) {
        if (i->head) {
            list_add(&up->list, i->head);
            return 0;
        }
        INIT_LIST_HEAD(&up->list);
        i->head = &up->list;
    }

    ret = request_irq(up->port.irq, serial8250_interrupt,
                      up->port.irqflags, up->port.name, i);
    if (ret < 0) {
        serial_do_unlink(i, up);
        return ret;
    }

    return 0;
}
```

## guard/scoped_guard 实现原理

v6 最终使用了 `guard(mutex)` 和 `scoped_guard(spinlock_irq)` 两种 scope-based 锁管理宏。
它们基于 GCC 的 `__attribute__((cleanup))` 编译器扩展，在变量离开作用域时自动调用析构函数。

### 核心机制：`__attribute__((cleanup))`

```c
// 当 var 离开作用域时，自动调用 destructor(&var)
type var __attribute__((cleanup(destructor))) = constructor(args);
```

这是 C 语言实现 RAII（Resource Acquisition Is Initialization）的基础，无需 C++ 的析构函数。

### DEFINE_LOCK_GUARD_1 展开

以 `guard(mutex)` 为例，mutex 的 guard 在 `include/linux/mutex.h:253` 中定义：

```c
DEFINE_LOCK_GUARD_1(mutex, struct mutex,
    mutex_lock(_T->lock),       // lock 语句
    mutex_unlock(_T->lock))     // unlock 语句
```

这个宏展开后生成：

```c
// 1. 定义一个结构体，包含锁指针
typedef struct {
    struct mutex *lock;
} class_mutex_t;

// 2. 构造函数：获取锁，lit = lock 指针
static __always_inline class_mutex_t
class_mutex_constructor(struct mutex *lock) {
    mutex_lock(lock);
    return (class_mutex_t){ .lock = lock };
}

// 3. 析构函数：释放锁，inline 展开
static __always_inline void
class_mutex_destructor(class_mutex_t *p) {
    struct mutex *lock = p->lock;
    mutex_unlock(lock);
}
```

### guard(mutex) 展开

```c
#define guard(_name)  CLASS(_name, __UNIQUE_ID(guard))

// CLASS(mutex, __UNIQUE_ID(guard)) 展开为：
class_mutex_t __UNIQUE_ID(guard)
    __attribute__((cleanup(class_mutex_destructor))) =
    class_mutex_constructor;
```

使用时 `guard(mutex)(&hash_mutex)` 等价于：

```c
class_mutex_t _guard_42
    __attribute__((cleanup(class_mutex_destructor))) =
    class_mutex_constructor(&hash_mutex);
```

当 `_guard_42` 离开作用域（函数返回、goto、break 等），编译器自动插入 `class_mutex_destructor(&_guard_42)` 调用，即 `mutex_unlock(&hash_mutex)`。

### scoped_guard 展开

`scoped_guard` 用于需要限定锁持有范围的场景。`i->lock` 必须在调用 `request_irq()`（可能睡眠）前释放，但 `hash_mutex` 仍需保持。`scoped_guard` 通过 for 循环技巧将锁的生命周期限制在紧随的 `{}` 块内：

```c
#define __scoped_guard(_name, _label, args...)                          \
    for (CLASS(_name, scope)(args);                                     \
         __guard_ptr(_name)(&scope) || !__is_cond_ptr(_name);           \
         ({ goto _label; }))                                            \
        if (0) {                                                        \
_label:                                                                 \
            break;                                                      \
        } else
```

使用时 `scoped_guard(spinlock_irq, &i->lock) { ... }` 等价于：

```c
// for 循环初始化：获取锁，scope 变量带有 cleanup 属性
for (class_spinlock_irq_t scope
         __attribute__((cleanup(class_spinlock_irq_destructor))) =
         class_spinlock_irq_constructor(&i->lock);
     // 条件：锁指针非空（无条件锁始终为真）
     __guard_ptr(spinlock_irq)(&scope) || !__is_cond_ptr(spinlock_irq);
     // 循环体结束后执行：goto _label 跳出循环
     ({ goto _label_42; }))
    if (0) {
_label_42:
        break;  // 永远不会执行，只是 label 的锚点
    } else
        // 用户的代码块在此
        { /* i->lock 保护的代码 */ }
```

执行流程：
1. **初始化**：构造 scope，获取 spinlock
2. **条件检查**：无条件锁 `!__is_cond_ptr` 为真，进入 else 分支
3. **执行用户代码块**
4. **循环迭代表达式**：`goto _label_42` 跳出循环
5. **scope 离开 for 循环作用域**：触发 `__attribute__((cleanup))` 析构，释放 spinlock

### 为什么高效

**1. 零运行时开销**

所有构造函数和析构函数都标记为 `__always_inline`，编译器将它们完全内联到调用点：

```c
// guard(mutex)(&hash_mutex) 编译后等同于：
mutex_lock(&hash_mutex);
// ... 函数体 ...
// 编译器在 return 前自动插入：
mutex_unlock(&hash_mutex);
```

与手写 `mutex_lock/unlock` 完全一致，没有任何额外函数调用。

**2. 自动匹配所有退出路径**

`__attribute__((cleanup))` 在变量离开作用域时自动触发，覆盖所有退出方式：
- `return` 语句
- `goto` 跳出
- `break` 跳出
- 函数正常结束

不需要在每个出口手动写 `unlock`，消除了遗漏释放锁的人为错误。

**3. 编译期安全**

`__no_context_analysis` 标记告诉编译器不要在析构函数上做上下文分析（避免死锁误报），但锁的获取/释放配对由编译器在代码生成层面保证，比手动配对更可靠。

**4. 与手写锁等价的代码生成**

以 `serial_link_irq_chain()` 为例，`guard(mutex)(&hash_mutex)` 生成的汇编代码与手写 `mutex_lock(&hash_mutex)` + 每个 return 前 `mutex_unlock(&hash_mutex)` 完全相同——编译器将 guard 变量完全优化掉，只保留 `mutex_lock` 和 `mutex_unlock` 调用。

## 版本变更总结

| 版本 | 变更 |
|------|------|
| V1 | 初始修复：将 hash_mutex 拉入 serial_link_irq_chain() |
| V2 | 添加 Reported-by 标签 |
| V3 | hash_mutex 跨越 request_irq() 完成 |
| V4 | request_irq() 失败时在 hash_mutex 下清理 |
| V5 | 添加 `__must_hold(&hash_mutex)` 注解 |
| V6 | 改回 guard(mutex)/scoped_guard 风格 |

## Lore 链接

- [v6](https://lore.kernel.org/r/20260624-bug-221579-8250-shared-irq-race-v6-1-xxx@gmail.com)（最新）
- [v5](https://lore.kernel.org/r/20260624-bug-221579-8250-shared-irq-race-v5-1-15d841f89e1e@gmail.com)
- [v4](https://lore.kernel.org/r/20260529-bug-221579-8250-shared-irq-race-v4-1-cfda63b4420f@gmail.com)
- [v3](https://lore.kernel.org/r/20260529-bug-221579-8250-shared-irq-race-v3-1-fe4d430862a9@gmail.com)
- [v2](https://lore.kernel.org/r/20260528-bug-221579-8250-shared-irq-race-v2-1-06531202e54d@gmail.com)
- [v1](https://lore.kernel.org/r/20260528-bug-221579-8250-shared-irq-race-v1-1-30980cca02f3@gmail.com)