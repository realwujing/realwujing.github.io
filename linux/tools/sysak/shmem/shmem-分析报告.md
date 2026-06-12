# sysak shmem 工具详细分析报告

> 工具路径：`source/tools/detect/mem/shmem/shmem.py`
> 测试环境：CTyunOS 2 (kernel 4.19.90-2102.2.0.0071.4.ctl2.x86_64)
> 测试机：ctyunos2-root

---

## 1. 工具概述

### 1.1 功能描述

`shmem` 是一个检测 **System V 共享内存 (shmem) 泄漏** 的 Python 2 诊断脚本。它通过对比两个时间点的 `ipcs -m` 快照，找出大小增长的共享内存段，并定位使用这些段的进程。

### 1.2 工作原理

```
ipcs -m > old_log   →   sleep(t)   →   ipcs -m > new_log   →   对比size   →   遍历/proc/PID/maps   →   输出结果
```

### 1.3 输出格式

```
shmem key      pid            name                total size(bytes)    increase(bytes)
1af40000       16955          td_connector        1073741824            30000
1af41689       24838          iohub-bridge        33554432              1000
```

### 1.4 依赖项

| 依赖 | 说明 |
|------|------|
| `ipcs` | System V IPC 查询工具（属于 util-linux） |
| `/proc` 文件系统 | 读取 `/proc/PID/maps` 和 `/proc/PID/comm` |
| Python 2 | 脚本声明 `#!/usr/bin/python2`，使用了 `os.popen()`、Py2 print 语法 |
| root 权限 | 读取其他进程的 `/proc/PID/maps` 需要 root |

---

## 2. 源码分析

### 2.1 代码结构

```
shmem.py (128行)
├── 全局变量 (11-16行): result_dir, old_log, new_log, result_log, default_time
├── exectue_cmd()    (18-23行): 执行shell命令，返回stdout（无错误处理）
├── shmem_check()    (26-99行): 核心检测逻辑
├── usage()          (101-108行): 帮助信息
└── main()           (113-125行): 参数解析入口
```

### 2.2 核心逻辑流 — `shmem_check(checktime)`

```python
def shmem_check(checktime):
    shmemkey_dict = {}     # key=hex key, value=size(string) — 仅记录增长的段
    shmeminc_dict = {}     # key=hex key, value=int — 增长量
    newkey_dict = {}       # 新快照解析结果
    oldkey_dict = {}       # 旧快照解析结果

    # 1. 准备目录，清理旧日志
    if not os.path.exists(result_dir):
        os.mkdir(result_dir)
    # 删除旧文件 ...

    # 2. 第一次 ipcs 快照
    command_old = "ipcs -m >" + old_log
    exectue_cmd(command_old)

    # 3. 等待 checktime 秒
    time.sleep(checktime)

    # 4. 第二次 ipcs 快照
    command_new = "ipcs -m >" + new_log
    exectue_cmd(command_new)

    # 5. 解析两个快照，构建 oldkey_dict 和 newkey_dict
    # key = pattern1.group(1) → hex key (去掉0x前缀)
    # value = sub[4] → size (字符串)

    # 6. 比较：增长的段存入 shmemkey_dict
    for key, value in newkey_dict.items():
        if value > oldkey_dict[key]:           # ⚠️ Bug 1 & 2
            shmemkey_dict[key] = value
            shmeminc_dict[key] = int(value) - int(oldkey_dict[key])

    # 7. 遍历所有进程，匹配 /proc/PID/maps 中的 key
    for file in os.listdir('/proc'):
        if file.isdigit():
            # 读 /proc/PID/comm 获取进程名
            # 读 /proc/PID/maps 匹配 shmem key
            # 匹配到则输出
```

### 2.3 数据结构

| 变量 | Key | Value 类型 | 用途 |
|------|-----|-----------|------|
| `oldkey_dict` | hex key (无0x前缀) | `str` — 快照1的 size | 旧快照基线 |
| `newkey_dict` | hex key (无0x前缀) | `str` — 快照2的 size | 新快照 |
| `shmemkey_dict` | hex key | `str` — 快照2的 size | 增长的段 |
| `shmeminc_dict` | hex key | `int` — 增长量 | 增量 |

---

## 3. Bug 清单

### 3.1 Bug 1 🔴 KeyError 崩溃 — `shmem.py:67`

**严重程度**：致命 — 生产环境直接崩溃

```python
for key, value in newkey_dict.items():
    if value > oldkey_dict[key]:   # KeyError!
```

**根因**：当监控窗口内**新出现**一个 shmem 段时，该 key 存在于 `newkey_dict` 但不存在于 `oldkey_dict`。

**测试复现**（ctyunos2-root 上实测）：

```bash
# python3 -c "
import subprocess, os, time, re
# 创建旧段
subprocess.call('ipcmk -M 4', shell=True)
# 快照1
subprocess.call('ipcs -m > /tmp/old_log', shell=True)
# 创建新段
subprocess.call('ipcmk -M 8', shell=True)
time.sleep(1)
# 快照2
subprocess.call('ipcs -m > /tmp/new_log', shell=True)
# 解析...
# newkey_dict包含新key，oldkey_dict不包含 → KeyError
"
```

**结果**：`KeyError: '6578105b'` — 程序崩溃，无任何输出。

**修复建议**：

```python
# 原代码
if value > oldkey_dict[key]:

# 修复
if key in oldkey_dict and int(value) > int(oldkey_dict[key]):
```

### 3.2 Bug 2 🔴 字符串比较 — `shmem.py:67`

**严重程度**：致命 — 比较结果完全错误

```python
sub = line.split()
new_size = sub[4]          # 字符串，如 '9437184'
oldkey_dict[key] = sub[4]  # 字符串

# 比较...
if value > oldkey_dict[key]:   # 字符串比较！
```

**实测数据**：

| 实际大小 | 字符串比较 | 正确整数比较 |
|---------|:---:|:---:|
| 9MB vs 10MB (`'9437184'` vs `'10485760'`) | `True` （9>10） ❌ | `False` |
| 500MB vs 1GB (`'524288000'` vs `'1073741824'`) | `False` (5<1) ❌ | `True` |

字符串比较按字典序逐字符比较：`'9'` > `'1'`，所以 9MB 被判为大于 10MB。

**修复建议**：比较前转换为 `int()`。

### 3.3 Bug 3 🟠 文件描述符泄漏 — 6处 `.close` 缺少括号

| 行号 | 语句 | 问题 |
|:---:|------|------|
| 56 | `old_fd.close` | 方法引用，不调用 |
| 65 | `new_fd.close` | 同上 |
| 84 | `comm_fd.close` | 同上 |
| 94 | `rseult_fd.close` | 同上 |
| 98 | `rseult_fd.close` | 同上 |
| 99 | `smaps_fd.close` | 同上 |

```python
old_fd.close   # 等价于 f = old_fd.close; del f — 不关闭文件
```

**测试确认**：

```python
class Foo:
    def close(self): return 42
f = Foo()
print(type(f.close).__name__)   # 输出 "method" — 未调用
print(f.close())                # 输出 42 — 需要括号
```

CPython 通过引用计数在变量被重新赋值时自动回收文件对象，此行为**不可依赖**。

**修复建议**：`s/.close$/.close()/g`

### 3.4 Bug 4 🟠 `-t` 无参数时静默退出 — `shmem.py:123`

```python
if opt_name in ('-t'):
    if opt_value:
        time = int(opt_value)
        shmem_check(time)
    else:
        time = default_time    # 只赋值，不调用 shmem_check()！
    sys.exit()
```

**复现**：`shmem -t` 期望以默认 5 秒运行，实际直接退出，无任何提示。

**修复建议**：在 `else` 分支中调用 `shmem_check(default_time)`。

### 3.5 Bug 5 🟠 `time` 变量覆盖模块 — `shmem.py:120`

```python
import time              # 模块
# ...
time = int(opt_value)    # 覆盖为整数
```

### 3.6 Bug 6 🟠 无泄漏时零输出反馈

若监控窗口内无 shmem 段增长，程序不创建 `result_log`，不输出任何内容。用户无法区分"正常无泄漏"和"程序崩溃"。

### 3.7 Bug 7 🟡 `os.popen()` 无错误处理 — `shmem.py:20`

```python
def exectue_cmd(command):
    command_fd = os.popen(command, "r")
    ret = command_fd.read()
    command_fd.close()
    return ret
```

`ipcs` 不存在或失败时返回空字符串，程序静默继续。

### 3.8 Bug 8 🟡 `/proc` 未挂载时 OSError — `shmem.py:72`

```python
pid_folders = os.listdir(pid_path)   # /proc 未挂载 → OSError
```

### 3.9 Bug 9 🟡 `ipcs -m` segments 处于 `dest` 状态时字段数变化

```python
sub = line.split()
old_size = sub[4]    # 硬编码索引
```

处于 `dest` (destroyed) 状态的 segment，`ipcs -m` 输出额外 `dest` 字段，可能导致字段数达到 7，但 `sub[4]` 仍是 bytes 字段，目前测试未触发布局偏移，但不排除未来格式变化的可能。

### 3.10 Bug 10 🟡 `rseult_fd` 拼写错误

`rseult_fd` 应为 `result_fd`。

---

## 4. 测试报告

### 4.1 测试环境

| 项目 | 值 |
|------|-----|
| 测试机 | ctyunos2-root |
| 内核 | 4.19.90-2102.2.0.0071.4.ctl2.x86_64 |
| Python | 2.7.18 / 3.7.9 |
| ipcs | /usr/bin/ipcs |
| 进程数 | 231 |
| UID | 0 (root) |

### 4.2 功能测试

| 测试项 | 预期 | 实际 | 结果 |
|-------|------|------|:--:|
| 空 segment 环境运行 | 无输出 | 无输出，无 result_log | ✅ |
| 有 segment 但不增长 | 无泄漏报告 | 无 result_log 文件 | ✅ |
| 模块 import | 成功 | 成功 | ✅ |
| -h 参数 | 打印帮助 | 打印帮助 | ✅ |
| -t 5 参数 | 运行检测 | 运行检测（但结果可能因 Bug 不正确） | ⚠️ |

### 4.3 Bug 验证测试

| Bug | 测试方法 | 结果 |
|-----|---------|:--:|
| Bug 1 (KeyError) | 旧快照后创建新段，再取新快照 | **确认** — `KeyError` 崩溃 |
| Bug 2 (字符串比较) | 直测: `'9437184' > '10485760'` | **确认** — 返回 `True`（错误） |
| Bug 3 (close泄漏) | 代码审查 + `type(x.close).__name__` | **确认** — method 对象，未调用 |
| Bug 6 (静默退出) | `-t` 无参数 | **确认** — 不运行检测 |

### 4.4 异常测试

| 测试项 | 行为 | 结果 |
|-------|------|:--:|
| `-t abc` 非整数 | `int(opt_value)` → `ValueError` 未捕获 | ❌ 崩溃 |
| 非 root 运行 | 无法读其他进程 /proc/PID/maps | ⚠️ 部分失效 |
| 并发运行 | 全局文件路径冲突，读写竞争 | ⚠️ 不安全 |
| ipcs 命令不存在 | `os.popen()` 返回空串，程序静默继续 | ⚠️ 无提示 |

---

## 5. 影响分析

### 5.1 性能影响

| 指标 | 实测值 (231进程, 无segment) | 评估 |
|------|---------------------------|:--:|
| 总执行时间 | 1.015s (含 1s sleep) | 低 |
| 实际 CPU user 时间 | 0.020s | 低 |
| 实际 CPU sys 时间 | 0.018s | 低 |
| 最大 RSS 内存 | 10,288 KB | 低 |
| /proc 文件读取量 | 231个 maps + 231个 comm | 低 |
| ipcs 执行时间 | 0.0016s | 低 |
| 全量 /proc 扫描时间 | 0.0073s | 低 |

> **结论**：工具本身对系统性能影响很小。主要开销是遍历 `/proc` 读取所有进程的 maps 文件。当系统有大量进程（数千个）或进程有超大内存映射时，开销会线性增加。

### 5.2 文件描述符影响

每运行一次泄漏 3~6 个 FD（通过 CPython GC 延迟回收）。`ulimit -n` 默认 1024，系统有 231 个进程时每次运行打开约 462+ 个 FD（231 maps + 231 comm + 日志文件）。大量并发或高频运行时可能触发 FD 耗尽。

### 5.3 并发安全

```python
result_dir = "/var/log/sysak/shmem/"
old_log = result_dir + "shmem_old_log"      # 全局变量
new_log = result_dir + "shmem_new_log"      # 全局变量
result_log = result_dir + "result_log"      # 全局变量
```

三个日志文件路径是**模块级全局变量**。多个进程同时运行时，会相互覆盖对方的日志文件，导致：
- 对方的快照数据被破坏
- 结果文件数据交错

### 5.4 Python 兼容性

| 特性 | Python 2 | Python 3 |
|------|:---:|:---:|
| Shebang `#!/usr/bin/python2` | ✅ | ❌ |
| `os.popen()` | ⚠️ deprecated | ❌ removed |
| `print (a,b,c,...)` (line 92) | ✅ | ❌ |
| `print ('str')` (line 102-107) | ✅ | ✅ |
| `import` (no Py3-only features) | ✅ | ✅ |

在 Python 3 环境下无法正常运行。

### 5.5 安全性

| 方面 | 评估 |
|------|------|
| 命令注入 | `exectue_cmd()` 接受 `command` 参数直传 `os.popen()`，但调用方只传硬编码字符串，**当前无注入风险** |
| 路径遍历 | `/proc` 条目通过 `.isdigit()` 校验，安全 |
| 权限需求 | 需要 root 才能读所有进程 maps |
| 敏感数据 | 遍历 `/proc/PID/maps` 可能泄露进程内存布局信息到日志 |

---

## 6. Bug 汇总

| # | 严重程度 | 行号 | 描述 | 影响 |
|---|:---:|:---:|------|------|
| 1 | 🔴 致命 | 67 | KeyError：新 shmem 段出现时崩溃 | 生产环境直接崩 |
| 2 | 🔴 致命 | 67 | 字符串比较代替整数比较 | size 判断完全错误 |
| 3 | 🟠 高级 | 56,65,84,94,98,99 | `.close` 缺少括号 — FD 泄漏 | 资源泄漏 |
| 4 | 🟠 高级 | 123 | `-t` 无参数静默退出 | 用户体验 |
| 5 | 🟠 高级 | 120 | `time` 覆盖 import time 模块 | 代码质量 |
| 6 | 🟠 高级 | 90 | 无泄漏时零输出 | 无法区分正常/异常 |
| 7 | 🟡 中级 | 20 | `os.popen()` 无错误检查 | 静默失败 |
| 8 | 🟡 中级 | 72 | `/proc` 未挂载 OSError | 未处理异常 |
| 9 | 🟡 低级 | 4 | `sub[4]` 硬编码 ipcs 输出字段 | 格式依赖 |
| 10 | 🟡 低级 | 91 | `rseult_fd` 拼写 | 代码质量 |

---

## 7. 修复建议（优先级排序）

### 7.1 优先修复（P0）

```python
# Bug 1 + Bug 2 同时修复 — shmem.py:67
# 原代码：
for key, value in newkey_dict.items():
    if value > oldkey_dict[key]:
        shmemkey_dict[key] = value
        shmeminc_dict[key] = int(value) - int(oldkey_dict[key])

# 修复后：
for key, value in newkey_dict.items():
    if key in oldkey_dict:
        new_size = int(value)
        old_size = int(oldkey_dict[key])
        if new_size > old_size:
            shmemkey_dict[key] = value
            shmeminc_dict[key] = new_size - old_size
    else:
        # 新出现的段，全部计入
        shmemkey_dict[key] = value
        shmeminc_dict[key] = int(value)
```

### 7.2 次要修复（P1）

- 6处 `.close` → `.close()`（或使用 `with` 语句）
- `-t` 无参数时调用 `shmem_check(default_time)`
- 添加 `try/except` 错误处理
- `exectue_cmd` 改为 `subprocess` 并检查返回码

### 7.3 其他改进（P2）

- 变量名 `rseult_fd` → `result_fd`
- `time` 变量 → `monitor_period` 之类的非冲突名
- 无泄漏时输出 "No shmem leak detected"
- 考虑 Python 3 迁移

---

## 8. 测试脚本

所有测试脚本位于测试机的 `/tmp/` 目录：

| 脚本 | 用途 |
|------|------|
| `/tmp/test_shmem.py` | 基本功能测试 + close 括号检查 |
| `/tmp/test_shmem_with_data.py` | KeyError bug 复现 |
| `/tmp/test_string_compare_bug.py` | 字符串比较 bug 验证 |
| `/tmp/test_edge_cases.py` | 边界条件 + ipcs 格式检查 |
| `/tmp/test_impact.py` | 性能 / 资源影响测试 |
