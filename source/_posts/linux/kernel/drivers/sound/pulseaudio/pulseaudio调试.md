---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# pulseaudio调试

## 源码下载

```bash
git clone https://github.com/realwujing/pulseaudio-12.2.58.git
```

## 安装依赖

```bash
sudo apt build-dep .
sudo apt install dh-make
```

## 编译调试包

### 关闭编译优化

```bash
cd pulseaudio-12.2.58
vim debian/rules
```

```bash
DEB_CFLAGS_MAINT_APPEND = -fstack-protector-strong -D_FORTITY_SOURCE=1 \
-z noexecstack -pie -fPIC -z lazy -g -O0
```

### 编译

```bash
#!/usr/bin/bash

set -x

dh_clean    # 调用makefile中的clean命令
rm ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.xz -rf    # 删除 dpkg-source -b . dh_make --createorig -sy 命令生成的源码压缩包
dh_make --createorig -sy    # 生成debian目录
dpkg-source -b .    # 生成构建源代码包
dpkg-buildpackage -uc -us -j16   # 编译制作deb包
```

### 安装调试包

```bash
sudo apt install --reinstall ../*.deb
```

## 开始调试

停止已有的pulseaudio进程:

```bash
systemctl --user stop pulseaudio.socket
systemctl --user stop pulseaudio
```

查看pulseaudio参数:

```bash
pulseaudio --help
```

```text
--log-level[=LEVEL]               Increase or set verbosity level
--log-meta[=BOOL]                 Include code location in log messages
--log-time[=BOOL]                 Include timestamps in log messages
--log-backtrace=FRAMES            Include a backtrace in log messages
```

gdb pusleaudio:

```bash
gdb /usr/bin/pusleaudio
set args --log-level=4 --log-meta=1 --log-time=1 --log-backtrace=FRAMES
```

也可以不关闭已有pulseaudio进程:

```bash
ps aux | grep pulseaudio
gdb -p 12567
file /usr/bin/pulseaudio
```

## main

```c
(gdb) bt
#0  main (argc=4, argv=0xffffffffdd98) at daemon/main.c:371
```

## pa_daemon_conf_load

/etc/pulse/daemon.conf加载流程

```c
(gdb) bt
#0  pa_daemon_conf_load (c=0xaaaaaaad5500, filename=0x0) at daemon/daemon-conf.c:603
#1  0x0000aaaaaaab7204 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:483
```

```c
int pa_daemon_conf_load(pa_daemon_conf *c, const char *filename) {
    int r = -1;
    FILE *f = NULL;
    struct channel_conf_info ci;
    pa_config_item table[] = {
        // 配置项名称，解析函数，存储值的指针，额外数据指针
        { "daemonize",                  pa_config_parse_bool,     &c->daemonize, NULL }, // 后台运行
        { "fail",                       pa_config_parse_bool,     &c->fail, NULL }, // 启动失败
        { "high-priority",              pa_config_parse_bool,     &c->high_priority, NULL }, // 高优先级
        // ... 其他配置项 ...
        { NULL,                         NULL,                     NULL, NULL }, // 配置项数组结束标记
    };

    pa_xfree(c->config_file);
    c->config_file = NULL;

    // 打开配置文件，根据提供的文件名或使用默认的文件名
    f = filename ?
        pa_fopen_cloexec(c->config_file = pa_xstrdup(filename), "r") :
        pa_open_config_file(DEFAULT_CONFIG_FILE, DEFAULT_CONFIG_FILE_USER, ENV_CONFIG_FILE, &c->config_file);

    if (!f && errno != ENOENT) {
        pa_log_warn(_("Failed to open configuration file: %s"), pa_cstrerror(errno));
        goto finish;
    }

    ci.default_channel_map_set = ci.default_sample_spec_set = false;
    ci.conf = c;

    // 解析配置文件并应用配置项
    r = f ? pa_config_parse(c->config_file, f, table, NULL, true, NULL) : 0;

    if (r >= 0) {
        // 确保通道映射和采样规格匹配

        if (ci.default_sample_spec_set &&
            ci.default_channel_map_set &&
            c->default_channel_map.channels != c->default_sample_spec.channels) {
            pa_log_error(_("The specified default channel map has a different number of channels than the specified default number of channels."));
            r = -1;
            goto finish;
        } else if (ci.default_sample_spec_set)
            pa_channel_map_init_extend(&c->default_channel_map, c->default_sample_spec.channels, PA_CHANNEL_MAP_DEFAULT);
        else if (ci.default_channel_map_set)
            c->default_sample_spec.channels = c->default_channel_map.channels;
    }

finish:
    if (f)
        fclose(f);

    return r;
}
```

### pa_open_config_file

```c
(gdb) bt
#0  pa_open_config_file (global=0xaaaaaaabd520 "/etc/pulse/daemon.conf", local=0xaaaaaaabd510 "/daemon.conf", env=0xaaaaaaabd500 "PULSE_CONFIG", result=0xaaaaaaad5560)
    at pulsecore/core-util.c:2021
#1  0x0000aaaaaaab43d4 in pa_daemon_conf_load (c=0xaaaaaaad5500, filename=0x0) at daemon/daemon-conf.c:710
#2  0x0000aaaaaaab7204 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:483
```

```c
/* 尝试打开配置文件。如果指定了 "env"，则打开指定环境变量的值。
 * 否则在家目录中查找文件 "local"，或者在全局文件系统中查找文件 "global"。
 * 如果 "result" 非空，则将指向新分配的包含所使用配置文件路径的缓冲区的指针存储在那里。 */
FILE *pa_open_config_file(const char *global, const char *local, const char *env, char **result) {
    const char *fn;
    FILE *f;

    // 如果指定了 "env" 并且获取到了对应环境变量的值，则尝试打开该文件
    if (env && (fn = getenv(env))) {
        if ((f = pa_fopen_cloexec(fn, "r"))) {
            if (result)
                *result = pa_xstrdup(fn);

            return f;
        }

        pa_log_warn("Failed to open configuration file '%s': %s", fn, pa_cstrerror(errno));
        return NULL;
    }

    // 尝试在本地目录打开文件
    if (local) {
        const char *e;
        char *lfn;
        char *h;

        // 如果设置了 "PULSE_CONFIG_PATH" 环境变量，则使用该路径
        if ((e = getenv("PULSE_CONFIG_PATH"))) {
            fn = lfn = pa_sprintf_malloc("%s" PA_PATH_SEP "%s", e, local);
            f = pa_fopen_cloexec(fn, "r");
        } else if ((h = pa_get_home_dir_malloc())) {
            // 尝试在家目录下的 .pulse 或 .config/pulse 目录打开文件
            fn = lfn = pa_sprintf_malloc("%s" PA_PATH_SEP ".pulse" PA_PATH_SEP "%s", h, local);
            f = pa_fopen_cloexec(fn, "r");
            if (!f) {
                free(lfn);
                fn = lfn = pa_sprintf_malloc("%s" PA_PATH_SEP ".config/pulse" PA_PATH_SEP "%s", h, local);
                f = pa_fopen_cloexec(fn, "r");
            }
            pa_xfree(h);
        } else
            return NULL;

        // 如果成功打开文件，则存储文件路径并返回文件指针
        if (f) {
            if (result)
                *result = pa_xstrdup(fn);

            pa_xfree(lfn);
            return f;
        }

        // 如果出错但错误码不是 ENOENT，则输出警告
        if (errno != ENOENT) {
            pa_log_warn("Failed to open configuration file '%s': %s", fn, pa_cstrerror(errno));
            pa_xfree(lfn);
            return NULL;
        }

        pa_xfree(lfn);
    }

    // 尝试在全局目录打开文件
    if (global) {
        char *gfn;

#ifdef OS_IS_WIN32
        if (strncmp(global, PA_DEFAULT_CONFIG_DIR, strlen(PA_DEFAULT_CONFIG_DIR)) == 0)
            gfn = pa_sprintf_malloc("%s" PA_PATH_SEP "etc" PA_PATH_SEP "pulse%s",
                                    pa_win32_get_toplevel(NULL),
                                    global + strlen(PA_DEFAULT_CONFIG_DIR));
        else
#endif
        gfn = pa_xstrdup(global);

        // 如果成功打开文件，则存储文件路径并返回文件指针
        if ((f = pa_fopen_cloexec(gfn, "r"))) {
            if (result)
                *result = gfn;
            else
                pa_xfree(gfn);

            return f;
        }
        pa_xfree(gfn);
    }

    // 如果无法打开文件，则设置错误码为 ENOENT 并返回 NULL
    errno = ENOENT;
    return NULL;
}
```

### pa_config_parse

```c
(gdb) bt
#0  pa_config_parse (filename=0xaaaaaaad5b20 "/etc/pulse/daemon.conf", f=0xaaaaaaad58f0, t=0xffffffffd188, proplist=0x0, use_dot_d=true, userdata=0x0) at pulsecore/conf-parser.c:168          
#1  0x0000aaaaaaab4484 in pa_daemon_conf_load (c=0xaaaaaaad5500, filename=0x0) at daemon/daemon-conf.c:720                                                                                     
#2  0x0000aaaaaaab7204 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:483
```

```c
/* 通过文件并逐行解析文件 */
int pa_config_parse(const char *filename, FILE *f, const pa_config_item *t, pa_proplist *proplist, bool use_dot_d,
                    void *userdata) {
    int r = -1;
    bool do_close = !f;
    pa_config_parser_state state;

    // 断言参数不为空
    pa_assert(filename);
    pa_assert(t);

    // 初始化状态结构
    pa_zero(state);

    // 如果未指定文件句柄，则尝试打开文件
    if (!f && !(f = pa_fopen_cloexec(filename, "r"))) {
        // 如果错误码为 ENOENT，则记录调试信息并返回 0
        if (errno == ENOENT) {
            pa_log_debug("Failed to open configuration file '%s': %s", filename, pa_cstrerror(errno));
            r = 0;
            goto finish;
        }

        // 否则记录警告信息并返回
        pa_log_warn("Failed to open configuration file '%s': %s", filename, pa_cstrerror(errno));
        goto finish;
    }
    pa_log_debug("Parsing configuration file '%s'", filename);

    // 初始化状态结构
    state.filename = filename;
    state.item_table = t;
    state.userdata = userdata;

    // 如果传入的 proplist 非空，则创建一个新的 proplist
    if (proplist)
        state.proplist = pa_proplist_new();

    // 逐行解析文件内容
    while (!feof(f)) {
        if (!fgets(state.buf, sizeof(state.buf), f)) {
            // 如果遇到文件结束，中断循环
            if (feof(f))
                break;

            // 否则记录警告信息并返回
            pa_log_warn("Failed to read configuration file '%s': %s", filename, pa_cstrerror(errno));
            goto finish;
        }

        state.lineno++;

        // 解析当前行
        if (parse_line(&state) < 0)
            goto finish;
    }

    // 如果传入的 proplist 非空，则更新主 proplist
    if (proplist)
        pa_proplist_update(proplist, PA_UPDATE_REPLACE, state.proplist);

    r = 0;

finish:
    // 释放临时 proplist
    if (state.proplist)
        pa_proplist_free(state.proplist);

    // 释放动态分配的资源
    pa_xfree(state.section);

    // 如果需要关闭文件，则关闭之
    if (do_close && f)
        fclose(f);

    // 如果使用了 dot_d，则在指定目录中继续解析
    if (use_dot_d) {
        // 在 Windows 平台下，通过 FindFirstFile() 和 FindNextFile() 遍历目录
        // 在其他平台下，通过 scandir() 遍历目录
#ifdef OS_IS_WIN32
        // Windows 平台下的处理
        // ...
#else
        // 其他平台下的处理
        // ...
#endif
    }

    return r;
}
```

#### parse_line

```c
(gdb) bt
#0  parse_line (state=0xffffffffc0d8) at pulsecore/conf-parser.c:86
#1  0x0000fffff7dad198 in pa_config_parse (filename=0xaaaaaaad5b20 "/etc/pulse/daemon.conf", f=0xaaaaaaad58f0, t=0xffffffffd188, proplist=0x0, use_dot_d=true, userdata=0x0)
    at pulsecore/conf-parser.c:208
#2  0x0000aaaaaaab4484 in pa_daemon_conf_load (c=0xaaaaaaad5500, filename=0x0) at daemon/daemon-conf.c:720
#3  0x0000aaaaaaab7204 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:483
```

```c
/* 解析变量赋值行 */
// static int parse_line(pa_config_parser_state *state) {
int parse_line(pa_config_parser_state *state) {
    char *c;

    // 跳过前导空格，获取 lvalue
    state->lvalue = state->buf + strspn(state->buf, WHITESPACE);
    pa_log("state->lvalue:%s", state->lvalue);

    // 查找并截断注释部分
    if ((c = strpbrk(state->lvalue, COMMENTS)))
        *c = 0;

    // 如果 lvalue 为空，则返回
    if (!*state->lvalue)
        return 0;

    // 处理 .include 指令
    if (pa_startswith(state->lvalue, ".include ")) {
        char *path = NULL, *fn;
        int r;

        // 提取文件名并检查是否为绝对路径
        fn = pa_strip(state->lvalue + 9);
        if (!pa_is_path_absolute(fn)) {
            const char *k;
            if ((k = strrchr(state->filename, '/'))) {
                // 构建文件的绝对路径
                char *dir = pa_xstrndup(state->filename, k - state->filename);
                fn = path = pa_sprintf_malloc("%s" PA_PATH_SEP "%s", dir, fn);
                pa_xfree(dir);
            }
        }

        // 解析所指定的文件
        r = pa_config_parse(fn, NULL, state->item_table, state->proplist, false, state->userdata);
        pa_xfree(path);
        return r;
    }

    // 处理节标题
    if (*state->lvalue == '[') {
        size_t k;

        k = strlen(state->lvalue);
        pa_assert(k > 0);

        // 检查节标题格式
        if (state->lvalue[k-1] != ']') {
            pa_log("[%s:%u] Invalid section header.", state->filename, state->lineno);
            return -1;
        }

        // 释放之前的节标题，设置新的节标题
        pa_xfree(state->section);
        state->section = pa_xstrndup(state->lvalue + 1, k-2);
        pa_log_debug("state->section:%s", state->section);

        // 检查是否进入 Properties 节
        if (pa_streq(state->section, "Properties")) {
            pa_log_debug("state->section:%s", state->section);
            if (!state->proplist) {
                pa_log("[%s:%u] \"Properties\" section is not allowed in this file.", state->filename, state->lineno);
                return -1;
            }

            state->in_proplist = true;
        } else
            state->in_proplist = false;

        return 0;
    }

    // 处理赋值操作
    if (!(state->rvalue = strchr(state->lvalue, '='))) {
        pa_log("[%s:%u] Missing '='.", state->filename, state->lineno);
        return -1;
    }

    // 截断 lvalue，并获取 rvalue
    *state->rvalue = 0;
    state->rvalue++;

    state->lvalue = pa_strip(state->lvalue);
    state->rvalue = pa_strip(state->rvalue);

    // 根据是否在属性列表中，执行不同的赋值操作
    if (state->in_proplist)
        return proplist_assignment(state);
    else
        return normal_assignment(state);
}
```

##### proplist_assignment

parse_line中有个关键函数proplist_assignment：

src/pulsecore/conf-parser.c:68

```c
/* 解析属性列表项。 */
static int proplist_assignment(pa_config_parser_state *state) {
    // 断言检查输入参数
    pa_assert(state);
    pa_assert(state->proplist);

    // 使用属性列表设置键值对
    if (pa_proplist_sets(state->proplist, state->lvalue, state->rvalue) < 0) {
        pa_log("[%s:%u] Failed to parse a proplist entry: %s = %s", state->filename, state->lineno, state->lvalue, state->rvalue);
        return -1;
    }

    return 0;
}
```

##### normal_assignment

parse_line中有个关键函数normal_assignment：

src/pulsecore/conf-parser.c：42

```c
/* 运行用户提供的解析器来处理赋值操作 */
static int normal_assignment(pa_config_parser_state *state) {
    const pa_config_item *item;

    pa_assert(state);

    // 遍历配置项表格，查找匹配的项
    for (item = state->item_table; item->parse; item++) {

        // 如果lvalue不为空且不匹配当前行的lvalue，则继续下一个循环
        if (item->lvalue && !pa_streq(state->lvalue, item->lvalue))
            continue;

        // 如果项需要指定section，但当前没有section，则继续下一个循环
        if (item->section && !state->section)
            continue;

        // 如果项需要指定section且当前section不匹配项的section，则继续下一个循环
        if (item->section && !pa_streq(state->section, item->section))
            continue;

        // 设置解析器的数据指针为当前项的数据
        state->data = item->data;

        // 调用项的解析函数处理当前行
        return item->parse(state);
    }

    // 如果没有匹配的项，则输出错误日志
    pa_log("[%s:%u] Unknown lvalue '%s' in section '%s'.", state->filename, state->lineno, state->lvalue, pa_strna(state->section));

    return -1;
}
```

`item->parse`是个回调函数，它的参数是一个 `pa_config_parser_state` 类型的指针，该指针指向一个 `pa_config_item` 类型的结构体。

src/daemon/daemon-conf.c：603

```c
pa_config_item table[] = {
    // 配置项名称，解析函数，存储值的指针，额外数据指针
    { "daemonize",                  pa_config_parse_bool,     &c->daemonize, NULL },
    { "fail",                       pa_config_parse_bool,     &c->fail, NULL },
    { "high-priority",              pa_config_parse_bool,     &c->high_priority, NULL },
    { "realtime-scheduling",        pa_config_parse_bool,     &c->realtime_scheduling, NULL },
    { "disallow-module-loading",    pa_config_parse_bool,     &c->disallow_module_loading, NULL },
    { "allow-module-loading",       pa_config_parse_not_bool, &c->disallow_module_loading, NULL },
    { "disallow-exit",              pa_config_parse_bool,     &c->disallow_exit, NULL },
    { "allow-exit",                 pa_config_parse_not_bool, &c->disallow_exit, NULL },
    { "use-pid-file",               pa_config_parse_bool,     &c->use_pid_file, NULL },
    { "system-instance",            pa_config_parse_bool,     &c->system_instance, NULL },
#ifdef HAVE_DBUS
    { "local-server-type",          parse_server_type,        c, NULL }, // 解析本地服务器类型
#endif
    { "no-cpu-limit",               pa_config_parse_bool,     &c->no_cpu_limit, NULL },
    { "cpu-limit",                  pa_config_parse_not_bool, &c->no_cpu_limit, NULL },
    { "disable-shm",                pa_config_parse_bool,     &c->disable_shm, NULL },
    { "enable-shm",                 pa_config_parse_not_bool, &c->disable_shm, NULL },
    { "enable-memfd",               pa_config_parse_not_bool, &c->disable_memfd, NULL },
    { "flat-volumes",               pa_config_parse_bool,     &c->flat_volumes, NULL },
    { "lock-memory",                pa_config_parse_bool,     &c->lock_memory, NULL },
    { "enable-deferred-volume",     pa_config_parse_bool,     &c->deferred_volume, NULL },
    { "exit-idle-time",             pa_config_parse_int,      &c->exit_idle_time, NULL },
    { "scache-idle-time",           pa_config_parse_int,      &c->scache_idle_time, NULL },
    { "realtime-priority",          parse_rtprio,             c, NULL }, // 解析实时优先级
    { "dl-search-path",             pa_config_parse_string,   &c->dl_search_path, NULL },
    { "default-script-file",        pa_config_parse_string,   &c->default_script_file, NULL },
    { "log-target",                 parse_log_target,         c, NULL }, // 解析日志目标
    { "log-level",                  parse_log_level,          c, NULL }, // 解析日志级别
    { "verbose",                    parse_log_level,          c, NULL },
    { "resample-method",            parse_resample_method,    c, NULL }, // 解析重采样方法
    { "default-sample-format",      parse_sample_format,      c, NULL }, // 解析默认采样格式
    { "default-sample-rate",        parse_sample_rate,        c, NULL }, // 解析默认采样率
    { "alternate-sample-rate",      parse_alternate_sample_rate, c, NULL },
    { "default-sample-channels",    parse_sample_channels,    &ci,  NULL }, // 解析默认采样通道数
    { "default-channel-map",        parse_channel_map,        &ci,  NULL }, // 解析默认通道映射
    { "default-fragments",          parse_fragments,          c, NULL }, // 解析默认碎片数
    { "default-fragment-size-msec", parse_fragment_size_msec, c, NULL }, // 解析默认碎片大小（毫秒）
    { "deferred-volume-safety-margin-usec",
                                    pa_config_parse_unsigned, &c->deferred_volume_safety_margin_usec, NULL }, // 解析延迟音量安全边界（微秒）
    { "deferred-volume-extra-delay-usec",
                                    pa_config_parse_int,      &c->deferred_volume_extra_delay_usec, NULL }, // 解析延迟音量额外延迟（微秒）
    { "nice-level",                 parse_nice_level,         c, NULL }, // 解析进程优先级
    { "avoid-resampling",           pa_config_parse_bool,     &c->avoid_resampling, NULL },
    { "disable-remixing",           pa_config_parse_bool,     &c->disable_remixing, NULL },
    { "enable-remixing",            pa_config_parse_not_bool, &c->disable_remixing, NULL },
    { "remixing-use-all-sink-channels",
                                    pa_config_parse_bool,     &c->remixing_use_all_sink_channels, NULL },
    { "disable-lfe-remixing",       pa_config_parse_bool,     &c->disable_lfe_remixing, NULL },
    { "enable-lfe-remixing",        pa_config_parse_not_bool, &c->disable_lfe_remixing, NULL },
    { "lfe-crossover-freq",         pa_config_parse_unsigned, &c->lfe_crossover_freq, NULL },
    { "load-default-script-file",   pa_config_parse_bool,     &c->load_default_script_file, NULL },
    { "shm-size-bytes",             pa_config_parse_size,     &c->shm_size, NULL },
    { "log-meta",                   pa_config_parse_bool,     &c->log_meta, NULL },
    { "log-time",                   pa_config_parse_bool,     &c->log_time, NULL },
    { "log-backtrace",              pa_config_parse_unsigned, &c->log_backtrace, NULL },
#ifdef HAVE_SYS_RESOURCE_H
    { "rlimit-fsize",               parse_rlimit,             &c->rlimit_fsize, NULL },
    { "rlimit-data",                parse_rlimit,             &c->rlimit_data, NULL },
    { "rlimit-stack",               parse_rlimit,             &c->rlimit_stack, NULL },
    { "rlimit-core",                parse_rlimit,             &c->rlimit_core, NULL },
#ifdef RLIMIT_RSS
    { "rlimit-rss",                 parse_rlimit,             &c->rlimit_rss, NULL },
#endif
#ifdef RLIMIT_NOFILE
    { "rlimit-nofile",              parse_rlimit,             &c->rlimit_nofile, NULL },
#endif
#ifdef RLIMIT_AS
    { "rlimit-as",                  parse_rlimit,             &c->rlimit_as, NULL },
#endif
#ifdef RLIMIT_NPROC
    { "rlimit-nproc",               parse_rlimit,             &c->rlimit_nproc, NULL },
#endif
#ifdef RLIMIT_MEMLOCK
    { "rlimit-memlock",             parse_rlimit,             &c->rlimit_memlock, NULL },
#endif
#ifdef RLIMIT_LOCKS
    { "rlimit-locks",               parse_rlimit,             &c->rlimit_locks, NULL },
#endif
#ifdef RLIMIT_SIGPENDING
    { "rlimit-sigpending",          parse_rlimit,             &c->rlimit_sigpending, NULL },
#endif
#ifdef RLIMIT_MSGQUEUE
    { "rlimit-msgqueue",            parse_rlimit,             &c->rlimit_msgqueue, NULL },
#endif
#ifdef RLIMIT_NICE
    { "rlimit-nice",                parse_rlimit,             &c->rlimit_nice, NULL },
#endif
#ifdef RLIMIT_RTPRIO
    { "rlimit-rtprio",              parse_rlimit,             &c->rlimit_rtprio, NULL },
#endif
#ifdef RLIMIT_RTTIME
    { "rlimit-rttime",              parse_rlimit,             &c->rlimit_rttime, NULL },
#endif
#endif
    { NULL,                         NULL,                     NULL, NULL }, // 配置项数组结束标记
};
```

## pa_get_hw_info

```bash
(gdb)
489         if (pa_get_hw_info(hw_info_path, card_name)) {
(gdb) s
pa_get_hw_info (path=0xaaaaaaabeac8 "/usr/share/uos-hw-config/hw_dmi_version", card=0xffffffffdb28 "") at daemon/daemon-conf.c:540                                                             
540     bool pa_get_hw_info(const char *path, char *card) {
(gdb) b 540
Breakpoint 30 at 0xaaaaaaab3674: file daemon/daemon-conf.c, line 540.
(gdb) bt
#0  pa_get_hw_info (path=0xaaaaaaabeac8 "/usr/share/uos-hw-config/hw_dmi_version", card=0xffffffffdb28 "") at daemon/daemon-conf.c:540                                                         
#1  0x0000aaaaaaab723c in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:489                                                                                                                            "uos-PC" 12:37 17-8月-23
(gdb) n
545         const char *state = NULL;
(gdb)
547         matched = false;
(gdb)
548         if ((s = pa_read_line_from_file(path)) != NULL) {
(gdb)
569         return matched;
(gdb)
570     }
(gdb) p matched
$10 = false
(gdb) fin
Run till exit from #0  pa_get_hw_info (path=0xaaaaaaabeac8 "/usr/share/uos-hw-config/hw_dmi_version", card=0xffffffffdb28 "") at daemon/daemon-conf.c:570                                      
0x0000aaaaaaab723c in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:489
489         if (pa_get_hw_info(hw_info_path, card_name)) {
Value returned is $11 = false
```

uos arm上没有`/usr/share/uos-hw-config/hw_dmi_version`文件，此函数返回false，故下方代码不执行：

src/daemon/main.c：489

```c
if (pa_get_hw_info(hw_info_path, card_name)) {
        pa_get_vendor_name(card_name, vendor_name);
        huawei_identification = true;
        if (pa_platform_load(conf, vendor_name, card_name) < 0)
            pa_log(_("Failed to load platform file."));
    }
```

```c
// 获取硬件信息，根据给定的路径和卡片名进行匹配
bool pa_get_hw_info(const char *path, char *card) {
    int i;
    bool matched;
    char *s;
    char *r;
    const char *state = NULL;

    // 初始化匹配状态为未匹配
    matched = false;
    
    // 从文件中读取一行信息
    if ((s = pa_read_line_from_file(path)) != NULL) {
        // 逐个处理空格分隔的信息
        while ((r = pa_split_spaces(s, &state))) {
            pa_log_debug("hw info r: %s", r);

            // 遍历硬件ID表格，查找匹配的板号
            for (i = 0; id_table[i].id != -1; i++) {
                if (strcmp(id_table[i].board, r) == 0) {
                    // 复制对应的卡片名到给定的缓冲区中
                    strncpy(card, id_table[i].card, sizeof(card));
                    // 设置匹配状态为已匹配
                    matched = true;
                    break;
                }
            }

            // 释放分隔后的字符串
            pa_xfree(r);

            // 如果匹配成功，则结束循环
            if (matched)
                break;
        }

        // 释放读取的行字符串
        pa_xfree(s);
    }

    // 返回匹配状态
    return matched;
}
```

## pa_daemon_conf_env

从环境变量中获取配置信息

```c
(gdb) n
496         if (pa_daemon_conf_env(conf) < 0)
(gdb) s
pa_daemon_conf_env (c=0xaaaaaaad5500) at daemon/daemon-conf.c:747
747         pa_assert(c);
(gdb) bt
#0  pa_daemon_conf_env (c=0xaaaaaaad5500) at daemon/daemon-conf.c:747
#1  0x0000aaaaaaab72b8 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:496
(gdb) fin
Run till exit from #0  pa_daemon_conf_env (c=0xaaaaaaad5500) at daemon/daemon-conf.c:758
main (argc=4, argv=0xffffffffdd98) at daemon/main.c:496
496         if (pa_daemon_conf_env(conf) < 0)
Value returned is $15 = 0
```

```c
// 从环境变量中获取配置信息并更新到pa_daemon_conf结构
int pa_daemon_conf_env(pa_daemon_conf *c) {
    char *e;
    pa_assert(c);

    // 从环境变量中获取DL搜索路径并更新到结构中
    if ((e = getenv(ENV_DL_SEARCH_PATH))) {
        pa_xfree(c->dl_search_path);
        c->dl_search_path = pa_xstrdup(e);
    }

    // 从环境变量中获取默认脚本文件路径并更新到结构中
    if ((e = getenv(ENV_SCRIPT_FILE))) {
        pa_xfree(c->default_script_file);
        c->default_script_file = pa_xstrdup(e);
    }

    // 返回0表示成功
    return 0;
}
```

## pa_cmdline_parse

解析命令行参数并更新pa_daemon_conf结构

```c
(gdb) n
499         if (pa_cmdline_parse(conf, argc, argv, &d) < 0) {
(gdb) s
pa_cmdline_parse (conf=0xaaaaaaad5500, argc=4, argv=0xffffffffdd98, d=0xffffffffda24) at daemon/cmdline.c:172                                                                                  
warning: Source file is more recent than executable.
172         pa_strbuf *buf = NULL;
(gdb) fin
Run till exit from #0  pa_cmdline_parse (conf=0xaaaaaaad5500, argc=4, argv=0xffffffffdd98, d=0xffffffffda24) at daemon/cmdline.c:172
main (argc=4, argv=0xffffffffdd98) at daemon/main.c:499
499         if (pa_cmdline_parse(conf, argc, argv, &d) < 0) {
Value returned is $16 = 0
```

```c
// 解析命令行参数并更新pa_daemon_conf结构
int pa_cmdline_parse(pa_daemon_conf *conf, int argc, char *const argv[], int *d) {
    pa_strbuf *buf = NULL;
    int c;
    int b;

    pa_assert(conf);
    pa_assert(argc > 0);
    pa_assert(argv);

    buf = pa_strbuf_new();

    if (conf->script_commands)
        pa_strbuf_puts(buf, conf->script_commands);

    while ((c = getopt_long(argc, argv, "L:F:ChDnp:kv", long_options, NULL)) != -1) {
        switch (c) {
            // 解析命令行参数并更新对应的配置命令
            case ARG_HELP:
            case 'h':
                conf->cmd = PA_CMD_HELP;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_VERSION:
                conf->cmd = PA_CMD_VERSION;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_DUMP_CONF:
                conf->cmd = PA_CMD_DUMP_CONF;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_DUMP_MODULES:
                conf->cmd = PA_CMD_DUMP_MODULES;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_DUMP_RESAMPLE_METHODS:
                conf->cmd = PA_CMD_DUMP_RESAMPLE_METHODS;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_CLEANUP_SHM:
                conf->cmd = PA_CMD_CLEANUP_SHM;
                break;

            // 解析命令行参数并更新对应的配置命令
            case 'k':
            case ARG_KILL:
                conf->cmd = PA_CMD_KILL;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_START:
                conf->cmd = PA_CMD_START;
                conf->daemonize = true;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_CHECK:
                conf->cmd = PA_CMD_CHECK;
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_LOAD:
            case 'L':
                pa_strbuf_printf(buf, "load-module %s\n", optarg);
                break;

            // 解析命令行参数并更新对应的配置命令
            case ARG_FILE:
            case 'F': {
                char *p;
                pa_strbuf_printf(buf, ".include %s\n", p = pa_make_path_absolute(optarg));
                pa_xfree(p);
                break;
            }

            // 解析命令行参数并更新对应的配置属性
            case 'C':
                pa_strbuf_puts(buf, "load-module module-cli exit_on_eof=1\n");
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_DAEMONIZE:
            case 'D':
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--daemonize expects boolean argument"));
                    goto fail;
                }
                conf->daemonize = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_FAIL:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--fail expects boolean argument"));
                    goto fail;
                }
                conf->fail = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case 'v':
            case ARG_LOG_LEVEL:
                // 解析命令行参数并更新对应的配置属性
                if (optarg) {
                    if (pa_daemon_conf_set_log_level(conf, optarg) < 0) {
                        pa_log(_("--log-level expects log level argument (either numeric in range 0..4 or one of debug, info, notice, warn, error)."));
                        goto fail;
                    }
                } else {
                    if (conf->log_level < PA_LOG_LEVEL_MAX-1)
                        conf->log_level++;
                }

                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_HIGH_PRIORITY:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--high-priority expects boolean argument"));
                    goto fail;
                }
                conf->high_priority = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_REALTIME:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--realtime expects boolean argument"));
                    goto fail;
                }
                conf->realtime_scheduling = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_DISALLOW_MODULE_LOADING:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disallow-module-loading expects boolean argument"));
                    goto fail;
                }
                conf->disallow_module_loading = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_DISALLOW_EXIT:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disallow-exit expects boolean argument"));
                    goto fail;
                }
                conf->disallow_exit = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_USE_PID_FILE:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--use-pid-file expects boolean argument"));
                    goto fail;
                }
                conf->use_pid_file = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case 'p':
            case ARG_DL_SEARCH_PATH:
                pa_xfree(conf->dl_search_path);
                conf->dl_search_path = pa_xstrdup(optarg);
                break;

            // 解析命令行参数并更新对应的配置属性
            case 'n':
                conf->load_default_script_file = false;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_LOG_TARGET:
                // 解析命令行参数并更新对应的配置属性
                if (pa_daemon_conf_set_log_target(conf, optarg) < 0) {
#ifdef HAVE_SYSTEMD_JOURNAL
                    pa_log(_("Invalid log target: use either 'syslog', 'journal','stderr' or 'auto' or a valid file name 'file:<path>', 'newfile:<path>'."));
#else
                    pa_log(_("Invalid log target: use either 'syslog', 'stderr' or 'auto' or a valid file name 'file:<path>', 'newfile:<path>'."));
#endif
                    goto fail;
                }
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_LOG_TIME:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--log-time expects boolean argument"));
                    goto fail;
                }
                conf->log_time = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_LOG_META:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--log-meta expects boolean argument"));
                    goto fail;
                }
                conf->log_meta = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_LOG_BACKTRACE:
                conf->log_backtrace = (unsigned) atoi(optarg);
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_EXIT_IDLE_TIME:
                conf->exit_idle_time = atoi(optarg);
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_SCACHE_IDLE_TIME:
                conf->scache_idle_time = atoi(optarg);
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_RESAMPLE_METHOD:
                // 解析命令行参数并更新对应的配置属性
                if (pa_daemon_conf_set_resample_method(conf, optarg) < 0) {
                    pa_log(_("Invalid resample method '%s'."), optarg);
                    goto fail;
                }
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_SYSTEM:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--system expects boolean argument"));
                    goto fail;
                }
                conf->system_instance = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_NO_CPU_LIMIT:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--no-cpu-limit expects boolean argument"));
                    goto fail;
                }
                conf->no_cpu_limit = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_DISABLE_SHM:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disable-shm expects boolean argument"));
                    goto fail;
                }
                conf->disable_shm = !!b;
                break;

            // 解析命令行参数并更新对应的配置属性
            case ARG_ENABLE_MEMFD:
                // 解析命令行参数并更新对应的配置属性
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--enable-memfd expects boolean argument"));
                    goto fail;
                }
                conf->disable_memfd = !b;
                break;

            // 默认情况：无效的参数，返回失败
            default:
                goto fail;
        }
    }

    pa_xfree(conf->script_commands);
    conf->script_commands = pa_strbuf_to_string_free(buf);

    *d = optind;

    return 0;

fail:
    if (buf)
        pa_strbuf_free(buf);

    return -1;
}
```

## pa_ltdl_init

初始化libtool库

```c
(gdb) bt
#0  pa_ltdl_init () at daemon/ltdl-bind-now.c:118
#1  0x0000aaaaaaab74fc in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:545
(gdb) fin
Run till exit from #0  pa_ltdl_init () at daemon/ltdl-bind-now.c:118
main (argc=4, argv=0xffffffffdd98) at daemon/main.c:546
546         ltdl_init = true;
```

```c
// 初始化libtool库，并在需要的情况下添加BIND_NOW加载器
void pa_ltdl_init(void) {

#ifdef PA_BIND_NOW
    // BIND_NOW宏已定义时，声明dlopen_loader指针
    const lt_dlvtable *dlopen_loader;
#endif

    // 初始化libtool库，确保动态加载器已被初始化
    pa_assert_se(lt_dlinit() == 0);

#ifdef PA_BIND_NOW
    // 如果BIND_NOW宏已定义，进行以下操作
    /* Already initialised */
    if (bindnow_loader)
        return;

    // 在已加载的加载器列表中查找lt_dlopen加载器
    if (!(dlopen_loader = lt_dlloader_find((char*) "lt_dlopen"))) {
        pa_log_warn(_("Failed to find original lt_dlopen loader."));
        return;
    }

    // 分配内存来保存新的加载器信息
    if (!(bindnow_loader = malloc(sizeof(lt_dlvtable)))) {
        pa_log_error(_("Failed to allocate new dl loader."));
        return;
    }

    // 复制原始加载器的信息到新的加载器中
    memcpy(bindnow_loader, dlopen_loader, sizeof(*bindnow_loader));
    bindnow_loader->name = "bind-now-loader";
    bindnow_loader->module_open = bind_now_open;
    bindnow_loader->module_close = bind_now_close;
    bindnow_loader->find_sym = bind_now_find_sym;
    bindnow_loader->priority = LT_DLLOADER_PREPEND;

    // 将新的BIND_NOW加载器添加为默认的模块加载器
    if (lt_dlloader_add(bindnow_loader) != 0) {
        pa_log_warn(_("Failed to add bind-now-loader."));
        free(bindnow_loader);
        bindnow_loader = NULL;
    }
#endif
}
```

## pa_mainloop_new

事件循环

```c
(gdb) 
1033        pa_assert_se(mainloop = pa_mainloop_new());
(gdb) s
pa_mainloop_new () at pulse/mainloop.c:453
warning: Source file is more recent than executable.
453         pa_init_i18n();
(gdb) fin
Run till exit from #0  pa_mainloop_new () at pulse/mainloop.c:453
0x0000aaaaaaab8a84 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1033
1033        pa_assert_se(mainloop = pa_mainloop_new());
Value returned is $28 = (pa_mainloop *) 0xaaaaaaadad60
```

```c
/* 创建一个新的主事件循环。*/
pa_mainloop *pa_mainloop_new(void) {
    pa_mainloop *m;  // 主事件循环结构体指针

    pa_init_i18n();  // 初始化国际化支持

    // 分配内存并将其初始化为零
    m = pa_xnew0(pa_mainloop, 1);

    // 创建唤醒管道，用于事件循环的通信
    if (pa_pipe_cloexec(m->wakeup_pipe) < 0) {
        pa_log_error("ERROR: cannot create wakeup pipe");
        pa_xfree(m);
        return NULL;
    }

    // 将唤醒管道的读写端设置为非阻塞模式
    pa_make_fd_nonblock(m->wakeup_pipe[0]);
    pa_make_fd_nonblock(m->wakeup_pipe[1]);

    m->rebuild_pollfds = true;  // 标记需要重建 pollfds

    // 初始化事件循环 API 和用户数据
    m->api = vtable;
    m->api.userdata = m;

    m->state = STATE_PASSIVE;  // 设置状态为被动

    m->poll_func_ret = -1;  // 初始化 poll_func_ret

    return m;  // 返回创建的主事件循环
}
```

## pa_core默认参数设置

src/daemon/main.c：1042

```c
c->default_sample_spec = conf->default_sample_spec;  // 设置默认采样规格
c->alternate_sample_rate = conf->alternate_sample_rate;  // 设置备用采样率
c->default_channel_map = conf->default_channel_map;  // 设置默认通道映射
c->default_n_fragments = conf->default_n_fragments;  // 设置默认片段数量
c->default_fragment_size_msec = conf->default_fragment_size_msec;  // 设置默认片段大小（毫秒）
c->deferred_volume_safety_margin_usec = conf->deferred_volume_safety_margin_usec;  // 设置延迟音量安全裕度（微秒）
c->deferred_volume_extra_delay_usec = conf->deferred_volume_extra_delay_usec;  // 设置延迟音量额外延迟（微秒）
c->lfe_crossover_freq = conf->lfe_crossover_freq;  // 设置低音炮分频频率
c->exit_idle_time = conf->exit_idle_time;  // 设置退出空闲时间
c->scache_idle_time = conf->scache_idle_time;  // 设置样本缓存空闲时间
c->resample_method = conf->resample_method;  // 设置重采样方法
c->realtime_priority = conf->realtime_priority;  // 设置实时优先级
c->realtime_scheduling = conf->realtime_scheduling;  // 设置实时调度
c->avoid_resampling = conf->avoid_resampling;  // 设置避免重采样
c->disable_remixing = conf->disable_remixing;  // 设置禁用混音
c->remixing_use_all_sink_channels = conf->remixing_use_all_sink_channels;  // 设置混音使用所有目标通道
c->disable_lfe_remixing = conf->disable_lfe_remixing;  // 设置禁用低音炮混音
c->deferred_volume = conf->deferred_volume;  // 设置延迟音量
c->running_as_daemon = conf->daemonize;  // 设置作为守护进程运行
c->disallow_exit = conf->disallow_exit;  // 设置禁止退出
c->flat_volumes = conf->flat_volumes;  // 设置平坦音量
c->huawei_identity = huawei_identification;  // 设置华为身份标识
```

## pa_cpu_init

cpu特性设置

```c
(gdb) n
1083        pa_cpu_init(&c->cpu_info);
(gdb) bt
#0  main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1083
(gdb) s
pa_cpu_init (cpu_info=0xaaaaaaae30e0) at pulsecore/cpu.c:25
warning: Source file is more recent than executable.
25          cpu_info->cpu_type = PA_CPU_UNDEFINED;
(gdb) fin
Run till exit from #0  pa_cpu_init (cpu_info=0xaaaaaaae30e0) at pulsecore/cpu.c:25
```

```c
void pa_cpu_init(pa_cpu_info *cpu_info) {
    cpu_info->cpu_type = PA_CPU_UNDEFINED;  // 初始化 CPU 类型为未定义
    /* don't force generic code, used for testing only */
    cpu_info->force_generic_code = false;  // 不强制使用通用代码，仅用于测试

    // 如果没有设置环境变量 "PULSE_NO_SIMD"
    if (!getenv("PULSE_NO_SIMD")) {
        // 尝试初始化 x86 架构的 CPU 特性
        if (pa_cpu_init_x86(&cpu_info->flags.x86))
            cpu_info->cpu_type = PA_CPU_X86;  // 设置 CPU 类型为 X86
        // 如果 x86 初始化失败，尝试初始化 ARM 架构的 CPU 特性
        else if (pa_cpu_init_arm(&cpu_info->flags.arm))
            cpu_info->cpu_type = PA_CPU_ARM;  // 设置 CPU 类型为 ARM
        // 初始化 ORC (Optimized Inner Loops Runtime Compiler) 特性
        pa_cpu_init_orc(*cpu_info);
    }

    // 初始化 CPU 重映射函数
    pa_remap_func_init(cpu_info);
    // 初始化混音函数
    pa_mix_func_init(cpu_info);
}
```

## pa_daemon_conf_open_default_script_file

default.pa加载

```bash
(gdb) bt
#0  pa_daemon_conf_open_default_script_file (c=0xaaaaaaad5500) at daemon/daemon-conf.c:782
#1  0x0000aaaaaaab9080 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1111
(gdb) s
pa_open_config_file (global=0xaaaaaaabd620 "/etc/pulse/default.pa", local=0xaaaaaaabd610 "/default.pa", env=0xaaaaaaabd5e8 "PULSE_SCRIPT", result=0xaaaaaaad5548) at pulsecore/core-util.c:2021
warning: Source file is more recent than executable.
2021        if (env && (fn = getenv(env))) {
(gdb) fin
Run till exit from #0  pa_open_config_file (global=0xaaaaaaabd620 "/etc/pulse/default.pa", local=0xaaaaaaabd610 "/default.pa", env=0xaaaaaaabd5e8 "PULSE_SCRIPT", result=0xaaaaaaad5548)
    at pulsecore/core-util.c:2021
0x0000aaaaaaab4884 in pa_daemon_conf_open_default_script_file (c=0xaaaaaaad5500) at daemon/daemon-conf.c:782
782                 f = pa_open_config_file(DEFAULT_SCRIPT_FILE, DEFAULT_SCRIPT_FILE_USER, ENV_SCRIPT_FILE, &c->default_script_file);
Value returned is $36 = (FILE *) 0xaaaaaaad58f0
(gdb) fin
Run till exit from #0  0x0000aaaaaaab4884 in pa_daemon_conf_open_default_script_file (c=0xaaaaaaad5500) at daemon/daemon-conf.c:782
0x0000aaaaaaab9080 in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1111
1111                if ((f = pa_daemon_conf_open_default_script_file(conf))) {
Value returned is $37 = (FILE *) 0xaaaaaaad58f0
```

```c
FILE *pa_daemon_conf_open_default_script_file(pa_daemon_conf *c) {
    FILE *f;
    pa_assert(c);  // 断言传入的配置参数 c 不为空

    // 如果没有设置默认脚本文件
    if (!c->default_script_file) {
        // 如果是系统实例，尝试打开默认的系统脚本文件
        if (c->system_instance)
            f = pa_open_config_file(DEFAULT_SYSTEM_SCRIPT_FILE, NULL, ENV_SCRIPT_FILE, &c->default_script_file);
        // 如果不是系统实例，尝试打开默认的用户脚本文件
        else
            f = pa_open_config_file(DEFAULT_SCRIPT_FILE, DEFAULT_SCRIPT_FILE_USER, ENV_SCRIPT_FILE, &c->default_script_file);
    } else
        // 如果设置了默认脚本文件，直接打开该文件
        f = pa_fopen_cloexec(c->default_script_file, "r");

    return f;  // 返回打开的文件指针
}
```

### pa_cli_command_execute_line_stateful

```c
int pa_cli_command_execute_line_stateful(pa_core *c, const char *s, pa_strbuf *buf, bool *fail, int *ifstate) {
    const char *cs;

    pa_assert(c);    // 断言传入的核心参数 c 不为空
    pa_assert(s);    // 断言传入的命令字符串 s 不为空
    pa_assert(buf);  // 断言传入的字符串缓冲 buf 不为空

    cs = s + strspn(s, whitespace);  // 跳过字符串开头的空格字符

    if (*cs == '#' || !*cs)
        return 0;  // 如果行是注释或者空行，直接返回

    else if (*cs == '.') {
        // 处理以点号开头的元命令
        if (!strcmp(cs, META_ELSE)) {
            // 处理 ".else" 元命令
            if (!ifstate || *ifstate == IFSTATE_NONE) {
                pa_strbuf_printf(buf, "Meta command %s is not valid in this context\n", cs);
                return -1;
            } else if (*ifstate == IFSTATE_TRUE)
                *ifstate = IFSTATE_FALSE;
            else
                *ifstate = IFSTATE_TRUE;
            return 0;
        } else if (!strcmp(cs, META_ENDIF)) {
            // 处理 ".endif" 元命令
            if (!ifstate || *ifstate == IFSTATE_NONE) {
                pa_strbuf_printf(buf, "Meta command %s is not valid in this context\n", cs);
                return -1;
            } else
                *ifstate = IFSTATE_NONE;
            return 0;
        }

        // 处理其他元命令
        if (ifstate && *ifstate == IFSTATE_FALSE)
            return 0;
        // ...
        // 处理其他元命令逻辑
        // ...

    } else {
        // 处理普通命令
        const struct command *command;
        int unknown = 1;
        size_t l;

        if (ifstate && *ifstate == IFSTATE_FALSE)
            return 0;

        l = strcspn(cs, whitespace);

        for (command = commands; command->name; command++) {
            // 检查命令是否匹配已知命令列表
            if (strlen(command->name) == l && !strncmp(cs, command->name, l)) {
                int ret;
                pa_tokenizer *t = pa_tokenizer_new(cs, command->args);
                pa_assert(t);
                // 调用命令处理函数进行处理
                ret = command->proc(c, t, buf, fail);
                pa_tokenizer_free(t);
                unknown = 0;

                if (ret < 0 && *fail)
                    return -1;

                break;
            }
        }

        // 如果命令未知
        if (unknown) {
            pa_strbuf_printf(buf, "Unknown command: %s\n", cs);
            if (*fail)
                return -1;
        }
    }

    return 0;  // 返回处理结果
}
```

#### pa_cli_command

`command->proc`是一个函数指针：

```bash
(gdb) p command->proc
$38 = (int (*)(pa_core *, pa_tokenizer *, pa_strbuf *, _Bool *)) 0xfffff7e4ffb0 <pa_cli_command_load>                                                                                          
(gdb) s
pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaaaddf90, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:422                                                               
422     static int pa_cli_command_load(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail) {                                                                                              
(gdb) bt
#0  pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaaaddf90, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:422
#1  0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-device-restore", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, 
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#2  0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#3  0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

##### command

src/pulsecore/cli-command.c:62

```c
struct command {
    const char *name;
    int (*proc) (pa_core *c, pa_tokenizer*t, pa_strbuf *buf, bool *fail);
    const char *help;
    unsigned args;
};
```

回调函数总览:

src/pulsecore/cli-command.c:82

```c
/* Prototypes for all available commands */
static int pa_cli_command_exit(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_help(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_modules(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_clients(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_cards(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sinks(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sources(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_inputs(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_outputs(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_stat(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_info(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_load(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_unload(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_describe(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_volume(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_input_volume(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_output_volume(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_volume(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_mute(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_mute(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_input_mute(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_output_mute(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_default(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_default(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_kill_client(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_kill_sink_input(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_kill_source_output(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_scache_play(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_scache_remove(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_scache_list(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_scache_load(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_scache_load_dir(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_play_file(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_dump(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_list_shared_props(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_move_sink_input(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_move_source_output(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_vacuum(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_suspend_sink(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_suspend_source(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_suspend(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_log_target(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_log_level(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_log_meta(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_log_time(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_log_backtrace(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_update_sink_proplist(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_update_source_proplist(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_update_sink_input_proplist(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_update_source_output_proplist(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_card_profile(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_sink_port(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_source_port(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_port_offset(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
static int pa_cli_command_dump_volumes(pa_core *c, pa_tokenizer *t, pa_strbuf *buf, bool *fail);
```

到此，pusleaudio整体框架基本介绍差不多了，下文会基于`command->proc`介绍`/usr/share/pulseaudio/alsa-mixer/profile-sets/`、`/usr/share/pulseaudio/alsa-mixer/paths/`部分。

## /usr/share/pulseaudio/alsa-mixer/profile-sets/

### pa_alsa_profile_set_new

解析profile-set

```c
(gdb) bt
#0  pa_config_parse (filename=0xaaaaaab154c0 "/usr/share/pulseaudio/alsa-mixer/profile-sets/default.conf", f=0x0, t=0xfffff2519640 <items>, proplist=0x0, use_dot_d=false, 
    userdata=0xaaaaaab28120) at pulsecore/conf-parser.c:168
#1  0x0000fffff24dcb04 in pa_alsa_profile_set_new (fname=0xfffff24fa608 "default.conf", bonus=0xaaaaaaae2f68) at modules/alsa/alsa-mixer.c:4577
#2  0x0000fffff268bc14 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:849
#3  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", 
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, 
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
pa_alsa_profile_set* pa_alsa_profile_set_new(const char *fname, const pa_channel_map *bonus) {
    pa_alsa_profile_set *ps; // 创建一个 pa_alsa_profile_set 结构体指针，用于存储配置文件中的配置信息
    pa_alsa_profile *p; // 创建一个 pa_alsa_profile 结构体指针，用于遍历配置文件中的各个音频配置
    pa_alsa_mapping *m; // 创建一个 pa_alsa_mapping 结构体指针，用于遍历配置文件中的各个音频映射
    pa_alsa_decibel_fix *db_fix; // 创建一个 pa_alsa_decibel_fix 结构体指针，用于遍历配置文件中的各个分贝修正
    char *fn; // 创建一个字符指针，用于存储配置文件的完整路径
    int r; // 用于存储函数返回值
    void *state; // 用于辅助遍历哈希表的状态指针

    // 配置项数组，用于解析配置文件中的各个配置项
    static pa_config_item items[] = {
        /* [General] */
        { "auto-profiles",          pa_config_parse_bool,         NULL, "General" },

        /* [Mapping ...] */
        { "device-strings",         mapping_parse_device_strings, NULL, NULL },
        { "channel-map",            mapping_parse_channel_map,    NULL, NULL },
        { "paths-input",            mapping_parse_paths,          NULL, NULL },
        { "paths-output",           mapping_parse_paths,          NULL, NULL },
        { "element-input",          mapping_parse_element,        NULL, NULL },
        { "element-output",         mapping_parse_element,        NULL, NULL },
        { "direction",              mapping_parse_direction,      NULL, NULL },
        { "exact-channels",         mapping_parse_exact_channels, NULL, NULL },

        /* Shared by [Mapping ...] and [Profile ...] */
        { "description",            mapping_parse_description,    NULL, NULL },
        { "priority",               mapping_parse_priority,       NULL, NULL },
        { "fallback",               mapping_parse_fallback,       NULL, NULL },

        /* [Profile ...] */
        { "input-mappings",         profile_parse_mappings,       NULL, NULL },
        { "output-mappings",        profile_parse_mappings,       NULL, NULL },
        { "skip-probe",             profile_parse_skip_probe,     NULL, NULL },

        /* [DecibelFix ...] */
        { "db-values",              decibel_fix_parse_db_values,  NULL, NULL },
        { NULL, NULL, NULL, NULL }
    };

    // 创建一个新的 pa_alsa_profile_set 结构体，将其中的各个哈希表进行初始化
    ps = pa_xnew0(pa_alsa_profile_set, 1);

    // 初始化音频映射、音频配置、分贝修正、路径的哈希表
    ps->mappings = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) mapping_free);
    ps->profiles = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) profile_free);
    ps->decibel_fixes = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) decibel_fix_free);
    ps->input_paths = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) pa_alsa_path_free);
    ps->output_paths = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) pa_alsa_path_free);

    // 初始化自定义路径的哈希表和文件夹
    ps->cust_paths = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, (pa_free_cb_t) pa_xfree, (pa_free_cb_t) pa_xfree);
    ps->cust_folder = NULL;

    // 配置项数组中的第一个配置项与 ps 结构体中的 auto_profiles 字段关联
    items[0].data = &ps->auto_profiles;

    // 如果没有提供配置文件名，则使用默认的配置文件名
    if (!fname)
        fname = "default.conf";

    // 构建配置文件的完整路径
    fn = pa_maybe_prefix_path(fname,
                              pa_run_from_build_tree() ? PA_SRCDIR "/modules/alsa/mixer/profile-sets/" :
                              PA_ALSA_PROFILE_SETS_DIR);

    // 解析配置文件并填充配置信息到 ps 结构体
    r = pa_config_parse(fn, NULL, items, NULL, false, ps);
    pa_xfree(fn);

    // 如果解析失败，则进行清理并返回 NULL
    if (r < 0)
        goto fail;

    // 遍历音频映射并进行验证
    PA_HASHMAP_FOREACH(m, ps->mappings, state)
        if (mapping_verify(m, bonus) < 0)
            goto fail;

    // 如果 auto_profiles 为真，则添加自动配置的音频配置
    if (ps->auto_profiles)
        profile_set_add_auto(ps);

    // 遍历音频配置并进行验证
    PA_HASHMAP_FOREACH(p, ps->profiles, state)
        if (profile_verify(p) < 0)
            goto fail;

    // 遍历分贝修正并进行验证
    PA_HASHMAP_FOREACH(db_fix, ps->decibel_fixes, state)
        if (decibel_fix_verify(db_fix) < 0)
            goto fail;

    return ps;

fail:
    // 如果解析或验证过程出现错误，则释放资源并返回 NULL
    pa_alsa_profile_set_free(ps);
    return NULL;
}
```

#### pa_config_parse

```c
src/modules/alsa/alsa-mixer.c:4577
src/pulsecore/conf-parser.c:167
```

参见上方。

##### parse_line

参见上方。

###### proplist_assignment

参见上方。

###### normal_assignment

参见上方。

#### profile_set_add_auto

自动将适合的音频配置添加到给定的 `pa_alsa_profile_set` 中

```c
(gdb) bt
#0  profile_set_add_auto (ps=0xaaaaaab28120) at modules/alsa/alsa-mixer.c:4306
#1  0x0000fffff24dcb8c in pa_alsa_profile_set_new (fname=0xfffff24fa608 "default.conf", bonus=0xaaaaaaae2f68) at modules/alsa/alsa-mixer.c:4588                                   
#2  0x0000fffff268bc14 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:849                                                                 
#3  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card",                                                        
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505,     
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/**
 * 自动将适合的音频配置添加到给定的 `pa_alsa_profile_set` 中。
 *
 * @param ps 要操作的 pa_alsa_profile_set 结构。
 */
static void profile_set_add_auto(pa_alsa_profile_set *ps) {
    pa_alsa_mapping *m, *n;
    void *m_state, *n_state;

    pa_assert(ps);

    /* 在这里的顺序很重要：
       1) 在尝试其组合之前，先尝试单独的输入和输出，因为如果半双工测试失败，我们不必尝试全双工。
       2) 尝试在与该输出组合的输入组合之前，因为在测试之间不会关闭 output_pcm。
    */
    PA_HASHMAP_FOREACH(n, ps->mappings, n_state)
        profile_set_add_auto_pair(ps, NULL, n);

    PA_HASHMAP_FOREACH(m, ps->mappings, m_state) {
        profile_set_add_auto_pair(ps, m, NULL);

        PA_HASHMAP_FOREACH(n, ps->mappings, n_state)
            profile_set_add_auto_pair(ps, m, n);
    }
}
```

##### profile_set_add_auto_pair

```c
(gdb) bt
#0  profile_set_add_auto_pair (ps=0xaaaaaab28120, m=0xaaaaaab28970, n=0xaaaaaab28970) at modules/alsa/alsa-mixer.c:4262                                                           
#1  0x0000fffff24dbc5c in profile_set_add_auto (ps=0xaaaaaab28120) at modules/alsa/alsa-mixer.c:4326                                                                              
#2  0x0000fffff24dcb8c in pa_alsa_profile_set_new (fname=0xfffff24fa608 "default.conf", bonus=0xaaaaaaae2f68) at modules/alsa/alsa-mixer.c:4588                                   
#3  0x0000fffff268bc14 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:849                                                                 
#4  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card",                                                        
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#5  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#6  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#7  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#8  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#9  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#10 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#11 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#12 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505,     
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#13 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#14 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static void profile_set_add_auto_pair(
        pa_alsa_profile_set *ps,
        pa_alsa_mapping *m, /* output */
        pa_alsa_mapping *n  /* input */) {

    char *name;
    pa_alsa_profile *p;

    pa_assert(ps);
    pa_assert(m || n);

    // 如果映射为输入，不添加配对
    if (m && m->direction == PA_ALSA_DIRECTION_INPUT)
        return;

    // 如果映射为输出，不添加配对
    if (n && n->direction == PA_ALSA_DIRECTION_OUTPUT)
        return;

    // 根据映射创建唯一配对名称
    if (m && n)
        name = pa_sprintf_malloc("output:%s+input:%s", m->name, n->name);
    else if (m)
        name = pa_sprintf_malloc("output:%s", m->name);
    else
        name = pa_sprintf_malloc("input:%s", n->name);

    // 如果已经存在同名的配置，不添加配对
    if (pa_hashmap_get(ps->profiles, name)) {
        pa_xfree(name);
        return;
    }

    // 创建新的alsa配置文件配置
    p = pa_xnew0(pa_alsa_profile, 1);
    p->profile_set = ps;
    p->name = name;

    // 处理输出映射
    if (m) {
        p->output_name = pa_xstrdup(m->name);
        p->output_mappings = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
        pa_idxset_put(p->output_mappings, m, NULL);
        p->priority += m->priority * 100;
        p->fallback_output = m->fallback;
    }

    // 处理输入映射
    if (n) {
        p->input_name = pa_xstrdup(n->name);
        p->input_mappings = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
        pa_idxset_put(p->input_mappings, n, NULL);
        p->priority += n->priority;
        p->fallback_input = n->fallback;
    }

    // 添加新配置到哈希表
    pa_hashmap_put(ps->profiles, p->name, p);
}
```

### pa_alsa_profile_set_cust_paths

将自定义path添加到profile_set哈希映射中

```c
Breakpoint 33, pa_alsa_profile_set_cust_paths (ps=0xaaaaaab28270, cust_folder=0xaaaaaac6a780 "cust/P710") at modules/alsa/alsa-mixer.c:4747                                       
warning: Source file is more recent than executable.
4747        pa_assert(ps);
(gdb) s
4748        pa_assert(cust_folder);
(gdb) bt
#0  pa_alsa_profile_set_cust_paths (ps=0xaaaaaab28270, cust_folder=0xaaaaaac6a780 "cust/P710") at modules/alsa/alsa-mixer.c:4748                                                  
#1  0x0000fffff268bcd0 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:868                                                                 
#2  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card",                                                        
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#3  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#4  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#5  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#6  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#7  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#8  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#9  0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#10 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505,     
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#11 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#12 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
(gdb) fin
Run till exit from #0  pa_alsa_profile_set_cust_paths (ps=0xaaaaaab28270, cust_folder=0xaaaaaac6a780 "cust/P710") at modules/alsa/alsa-mixer.c:4748                               
(  13.293|  11.844) D: [pulseaudio][modules/alsa/alsa-mixer.c:4751 pa_alsa_profile_set_cust_paths()] Analyzing directory: '/usr/share/pulseaudio/alsa-mixer/paths'                
(  13.293|   0.000) D: [pulseaudio][modules/alsa/alsa-mixer.c:4755 pa_alsa_profile_set_cust_paths()] open directory: '/usr/share/pulseaudio/alsa-mixer/paths/cust/P710'           
(  13.293|   0.000) W: [pulseaudio][modules/alsa/alsa-mixer.c:4758 pa_alsa_profile_set_cust_paths()] Failed to open /usr/share/pulseaudio/alsa-mixer/paths/cust/P710 
```

```c
void pa_alsa_profile_set_cust_paths(pa_alsa_profile_set *ps, const char *cust_folder) {

    DIR *dir;
    struct dirent *ent;
    const char *paths_dir;
    char *fn;
    char *key;
    char *value;

    pa_assert(ps);
    pa_assert(cust_folder);

    // 获取默认路径目录
    paths_dir = get_default_paths_dir();
    pa_log_debug("Analyzing directory: '%s'", paths_dir);

    // 构建路径名称
    fn = pa_maybe_prefix_path(cust_folder, paths_dir);

    pa_log_debug("open directory: '%s'", fn);

    // 打开目录并遍历文件
    if (!(dir = opendir(fn))) {
        pa_log_warn("Failed to open %s", fn);
        pa_xfree(fn);
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        // 忽略 . 和 .. 目录
        if (pa_streq(ent->d_name, ".") || pa_streq(ent->d_name, ".."))
            continue;
        pa_log_debug("Analyzing file: '%s'", ent->d_name);

        // 使用文件名作为键，构建完整路径作为值
        key = pa_xstrndup(ent->d_name, strlen(ent->d_name));
        value = pa_maybe_prefix_path(key, cust_folder);

        // 将路径添加到自定义路径的哈希映射中
        pa_hashmap_put(ps->cust_paths, key, value);
        pa_log_debug("ps->cust_paths file: '%s', '%s'", key, value);

    }
    closedir(dir);

    pa_xfree(fn);

}
```

### pa_alsa_profile_set_probe

探测pcm是否可用

```c
#0  pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4787
#1  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#2  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", 
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#3  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#4  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#5  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#6  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#7  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#8  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#9  0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#10 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, 
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#11 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#12 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
void pa_alsa_profile_set_probe(
        pa_alsa_profile_set *ps,               // ALSA 音频配置集合
        const char *dev_id,                    // 设备标识符
        const pa_sample_spec *ss,               // 采样规格
        unsigned default_n_fragments,           // 默认分片数
        unsigned default_fragment_size_msec) {  // 默认分片大小（毫秒）

    bool found_output = false, found_input = false;  // 是否找到输出和输入配置

    pa_alsa_profile *p, *last = NULL;      // 当前配置和上一个配置
    pa_alsa_profile **pp, **probe_order;   // 配置探测顺序数组
    pa_alsa_mapping *m;                     // ALSA 映射
    pa_hashmap *broken_inputs, *broken_outputs, *used_paths;  // 存储损坏输入、损坏输出和已使用路径的哈希映射
    pa_alsa_mapping *selected_fallback_input = NULL, *selected_fallback_output = NULL;  // 选定的回退输入和输出映射

    pa_assert(ps);           // 断言配置集合存在
    pa_assert(dev_id);       // 断言设备标识符存在
    pa_assert(ss);           // 断言采样规格存在

    // 创建哈希映射来存储损坏的输入、输出和已使用的路径
    broken_inputs = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
    broken_outputs = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
    used_paths = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    // 创建用于指示探测顺序的 probe_order 数组
    pp = probe_order = pa_xnew0(pa_alsa_profile *, pa_hashmap_size(ps->profiles) + 1);

    // 将配置按一定的优先级顺序添加到探测顺序中
    pp += add_profiles_to_probe(pp, ps->profiles, false, false);
    pp += add_profiles_to_probe(pp, ps->profiles, false, true);
    pp += add_profiles_to_probe(pp, ps->profiles, true, false);
    pp += add_profiles_to_probe(pp, ps->profiles, true, true);

    // 遍历探测顺序中的配置
    for (pp = probe_order; *pp; pp++) {
        uint32_t idx;
        p = *pp;

        // 跳过已经找到的回退配置，但仍然探测已选的回退配置
        if (found_input && p->fallback_input)
            if (selected_fallback_input == NULL || pa_idxset_get_by_index(p->input_mappings, 0) != selected_fallback_input)
                continue;
        if (found_output && p->fallback_output)
            if (selected_fallback_output == NULL || pa_idxset_get_by_index(p->output_mappings, 0) != selected_fallback_output)
                continue;

        // 跳过已经标记为支持的配置
        if (!p->supported) {
            // 最终化配置探测
            profile_finalize_probing(last, p);
            p->supported = true;

            // 检查输出映射是否能够打开
            if (p->output_mappings) {
                PA_IDXSET_FOREACH(m, p->output_mappings, idx) {
                    if (pa_hashmap_get(broken_outputs, m) == m) {
                        // 标记为不支持的配置
                        pa_log_debug("Skipping profile %s - will not be able to open output:%s", p->name, m->name);
                        p->supported = false;
                        break;
                    }
                }
            }

            // 检查输入映射是否能够打开
            if (p->input_mappings && p->supported) {
                PA_IDXSET_FOREACH(m, p->input_mappings, idx) {
                    if (pa_hashmap_get(broken_inputs, m) == m) {
                        // 标记为不支持的配置
                        pa_log_debug("Skipping profile %s - will not be able to open input:%s", p->name, m->name);
                        p->supported = false;
                        break;
                    }
                }
            }

            if (p->supported)
                pa_log_debug("Looking at profile %s", p->name);

            // 检查是否可以打开所有新的映射
            if (p->output_mappings && p->supported)
                PA_IDXSET_FOREACH(m, p->output_mappings, idx) {
                    // 检查是否已经打开
                    if (m->output_pcm)
                        continue;

                    // 尝试打开 PCM 设备
                    pa_log_debug("Checking for playback on %s (%s)", m->description, m->name);
                    if (!(m->output_pcm = mapping_open_pcm(m, ss, dev_id, m->exact_channels,
                                                           SND_PCM_STREAM_PLAYBACK,
                                                           default_n_fragments,
                                                           default_fragment_size_msec))) {
                        // 标记为不支持的配置
                        p->supported = false;
                        if (pa_idxset_size(p->output_mappings) == 1 &&
                            ((!p->input_mappings) || pa_idxset_size(p->input_mappings) == 0)) {
                            pa_log_debug("Caching failure to open output:%s", m->name);
                            pa_hashmap_put(broken_outputs, m, m);
                        }
                        break;
                    }

                    if (m->hw_device_index < 0)
                        mapping_query_hw_device(m, m->output_pcm);
                }

            // 检查输入映射是否能够打开
            if (p->input_mappings && p->supported)
                PA_IDXSET_FOREACH(m, p->input_mappings, idx) {
                    // 检查是否已经打开
                    if (m->input_pcm)
                        continue;

                    // 尝试打开 PCM 设备
                    pa_log_debug("Checking for recording on %s (%s)", m->description, m->name);
                    if (!(m->input_pcm = mapping_open_pcm(m, ss, dev_id, m->exact_channels,
                                                          SND_PCM_STREAM_CAPTURE,
                                                          default_n_fragments,
                                                          default_fragment_size_msec))) {
                        // 标记为不支持的配置
                        p->supported = false;
                        if (pa_idxset_size(p->input_mappings) == 1 &&
                            ((!p->output_mappings) || pa_idxset_size(p->output_mappings) == 0)) {
                            pa_log_debug("Caching failure to open input:%s", m->name);
                            pa_hashmap_put(broken_inputs, m, m);
                        }
                        break;
                    }

                    if (m->hw_device_index < 0)
                        mapping_query_hw_device(m, m->input_pcm);
                }

            last = p;

            if (!p->supported)
                continue;
        }

        pa_log_debug("Profile %s supported.", p->name);

        // 标记已找到输出和输入配置
        if (p->output_mappings)
            PA_IDXSET_FOREACH(m, p->output_mappings, idx)
                if (m->output_pcm) {
                    found_output = true;
                    if (p->fallback_output && selected_fallback_output == NULL) {
                        selected_fallback_output = m;
                    }
                    mapping_paths_probe(m, p, PA_ALSA_DIRECTION_OUTPUT, used_paths);
                }

        if (p->input_mappings)
            PA_IDXSET_FOREACH(m, p->input_mappings, idx)
                if (m->input_pcm) {
                    found_input = true;
                    if (p->fallback_input && selected_fallback_input == NULL) {
                        selected_fallback_input = m;
                    }
                    mapping_paths_probe(m, p, PA_ALSA_DIRECTION_INPUT, used_paths);
                }
    }

    // 最终化配置探测，清理资源
    profile_finalize_probing(last, NULL);

    // 丢弃不支持的配置
    pa_alsa_profile_set_drop_unsupported(ps);

    // 丢弃未使用的路径
    paths_drop_unused(ps->input_paths, used_paths);
    paths_drop_unused(ps->output_paths, used_paths);

    // 释放资源
    pa_hashmap_free(broken_inputs);
    pa_hashmap_free(broken_outputs);
    pa_hashmap_free(used_paths);
    pa_xfree(probe_order);

    ps->probed = true; // 标记为已完成探测
}
```

#### add_profiles_to_probe

```c
(gdb) bt
#0  add_profiles_to_probe (list=0xaaaaaac6bd50, profiles=0xaaaaaab292a0, fallback_output=false, fallback_input=false) at modules/alsa/alsa-mixer.c:4705                           
#1  0x0000fffff24dd90c in pa_alsa_profile_set_probe (ps=0xaaaaaab280c0, dev_id=0xaaaaaab123f0 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)       
    at modules/alsa/alsa-mixer.c:4809
#2  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f3f0) at modules/alsa/module-alsa-card.c:871                                                                 
#3  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card",                                                        
    argument=0xaaaaaaade630 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab036a0, d=0xaaaaaab07190) at modules/module-udev-detect.c:333                                                                  
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab036a0, dev=0xaaaaaaadda70) at modules/module-udev-detect.c:422                                                                 
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab036a0, dev=0xaaaaaaadda70) at modules/module-udev-detect.c:464                                                               
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab036a0, path=0xaaaaaab18aa0 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00dc0) at modules/module-udev-detect.c:789                                                                  
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab01150 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab01170, buf=0xaaaaaaadbed0, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadbed0, fail=0xaaaaaaad5505,     
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadbed0, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static int add_profiles_to_probe(
        pa_alsa_profile **list,       // 存储配置指针的列表
        pa_hashmap *profiles,          // ALSA 音频配置哈希映射
        bool fallback_output,          // 是否是回退输出配置
        bool fallback_input) {         // 是否是回退输入配置

    int i = 0;                       // 配置计数器
    void *state;
    pa_alsa_profile *p;

    // 遍历配置哈希映射，根据回退设置筛选配置
    PA_HASHMAP_FOREACH(p, profiles, state)
        if (p->fallback_input == fallback_input && p->fallback_output == fallback_output) {
            *list = p;                 // 将符合条件的配置指针添加到列表中
            list++;
            i++;
        }
    return i;                         // 返回筛选出的配置数量
}
```

#### mapping_open_pcm

```c
(gdb) bt
#0  mapping_open_pcm (m=0xaaaaaab28ba0, ss=0xaaaaaaae30fc, dev_id=0xaaaaaab12550 "0", exact_channels=true, mode=1, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4657
#1  0x0000fffff24ddf28 in pa_alsa_profile_set_probe (ps=0xaaaaaab28350, dev_id=0xaaaaaab12550 "0", ss=0xaaaaaaae30fc, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4891
#2  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f550) at modules/alsa/module-alsa-card.c:871
#3  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2fd0, name=0xfffff26a4d98 "module-alsa-card", 
    argument=0xaaaaaaadd690 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03800, d=0xaaaaaab072f0) at modules/module-udev-detect.c:333
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03800, dev=0xaaaaaaaddee0) at modules/module-udev-detect.c:422
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab03800, dev=0xaaaaaaaddee0) at modules/module-udev-detect.c:464
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab03800, path=0xaaaaaab18c00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00f20) at modules/module-udev-detect.c:789
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2fd0, name=0xaaaaaab012b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2fd0, t=0xaaaaaab012d0, buf=0xaaaaaaadb270, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2fd0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadb270, fail=0xaaaaaaad5505, 
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2fd0, f=0xaaaaaaad58f0, buf=0xaaaaaaadb270, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static snd_pcm_t* mapping_open_pcm(pa_alsa_mapping *m,
                                   const pa_sample_spec *ss,
                                   const char *dev_id,
                                   bool exact_channels,
                                   int mode,
                                   unsigned default_n_fragments,
                                   unsigned default_fragment_size_msec) {

    snd_pcm_t* handle;
    pa_sample_spec try_ss = *ss;
    pa_channel_map try_map = m->channel_map;
    snd_pcm_uframes_t try_period_size, try_buffer_size;

    // 尝试使用匹配通道数的采样规格
    try_ss.channels = try_map.channels;

    // 计算尝试的周期大小和缓冲区大小
    try_period_size =
        pa_usec_to_bytes(default_fragment_size_msec * PA_USEC_PER_MSEC, &try_ss) /
        pa_frame_size(&try_ss);
    try_buffer_size = default_n_fragments * try_period_size;

    // 使用模板尝试打开 ALSA 设备
    handle = pa_alsa_open_by_template(
                              m->device_strings, dev_id, NULL, &try_ss,
                              &try_map, mode, &try_period_size,
                              &try_buffer_size, 0, NULL, NULL, exact_channels);

    // 如果成功打开设备并且通道数有变化，则更新通道映射
    if (handle && !exact_channels && m->channel_map.channels != try_map.channels) {
        char buf[PA_CHANNEL_MAP_SNPRINT_MAX];
        pa_log_debug("Channel map for mapping '%s' permanently changed to '%s'", m->name,
                     pa_channel_map_snprint(buf, sizeof(buf), &try_map));
        m->channel_map = try_map;
    }

    return handle;
}
```

##### pa_alsa_open_by_template

```c
(gdb) bt
#0  pa_alsa_open_by_template (template=0xaaaaaab154c0, dev_id=0xaaaaaab125e0 "0", dev=0x0, ss=0xffffffffc940, map=0xffffffffc950, mode=1, period_size=0xffffffffc928, 
    buffer_size=0xffffffffc930, tsched_size=0, use_mmap=0x0, use_tsched=0x0, require_exact_channel_number=true) at modules/alsa/alsa-util.c:791
#1  0x0000fffff24dcfd0 in mapping_open_pcm (m=0xaaaaaab28b00, ss=0xaaaaaaae30fc, dev_id=0xaaaaaab125e0 "0", exact_channels=true, mode=1, default_n_fragments=4, 
    default_fragment_size_msec=25) at modules/alsa/alsa-mixer.c:4671
#2  0x0000fffff24ddf28 in pa_alsa_profile_set_probe (ps=0xaaaaaab282b0, dev_id=0xaaaaaab125e0 "0", ss=0xaaaaaaae30fc, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4891
#3  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f5e0) at modules/alsa/module-alsa-card.c:871
#4  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2fd0, name=0xfffff26a4d98 "module-alsa-card", 
    argument=0xaaaaaaadd9b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#5  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03890, d=0xaaaaaab07380) at modules/module-udev-detect.c:333
#6  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03890, dev=0xaaaaaaaddcd0) at modules/module-udev-detect.c:422
#7  0x0000fffff26a36bc in process_device (u=0xaaaaaab03890, dev=0xaaaaaaaddcd0) at modules/module-udev-detect.c:464
#8  0x0000fffff26a373c in process_path (u=0xaaaaaab03890, path=0xaaaaaab18c90 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#9  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00fb0) at modules/module-udev-detect.c:789
#10 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2fd0, name=0xaaaaaab01340 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#11 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2fd0, t=0xaaaaaab01360, buf=0xaaaaaaad7e70, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#12 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2fd0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaad7e70, fail=0xaaaaaaad5505, 
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#13 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2fd0, f=0xaaaaaaad58f0, buf=0xaaaaaaad7e70, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#14 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
snd_pcm_t *pa_alsa_open_by_template(
        char **template,
        const char *dev_id,
        char **dev,
        pa_sample_spec *ss,
        pa_channel_map* map,
        int mode,
        snd_pcm_uframes_t *period_size,
        snd_pcm_uframes_t *buffer_size,
        snd_pcm_uframes_t tsched_size,
        bool *use_mmap,
        bool *use_tsched,
        bool require_exact_channel_number) {

    snd_pcm_t *pcm_handle;
    char **i;

    // 遍历模板列表尝试打开 ALSA 设备
    for (i = template; *i; i++) {
        char *d;

        // 替换模板中的占位符，最终d指向了某个pcm设备，如hw0
        d = pa_replace(*i, "%f", dev_id);

        // 通过设备字符串打开 ALSA 设备
        pcm_handle = pa_alsa_open_by_device_string(
                d,
                dev,
                ss,
                map,
                mode,
                period_size,
                buffer_size,
                tsched_size,
                use_mmap,
                use_tsched,
                require_exact_channel_number);

        pa_xfree(d);

        // 如果成功打开设备，则返回 PCM 句柄
        if (pcm_handle)
            return pcm_handle;
    }

    // 未能成功打开设备，返回 NULL
    return NULL;
}
```

###### pa_alsa_open_by_device_string

```c
(gdb) bt
#0  pa_alsa_open_by_device_string (device=0xaaaaaac6a790 "hw:0", dev=0x0, ss=0xffffffffc940, map=0xffffffffc950, mode=1, period_size=0xffffffffc928, buffer_size=0xffffffffc930,  
    tsched_size=0, use_mmap=0x0, use_tsched=0x0, require_exact_channel_number=true) at modules/alsa/alsa-util.c:686                                                               
#1  0x0000fffff24bf87c in pa_alsa_open_by_template (template=0xaaaaaab15330, dev_id=0xaaaaaab12450 "0", dev=0x0, ss=0xffffffffc940, map=0xffffffffc950, mode=1,                   
    period_size=0xffffffffc928, buffer_size=0xffffffffc930, tsched_size=0, use_mmap=0x0, use_tsched=0x0, require_exact_channel_number=true) at modules/alsa/alsa-util.c:796       
#2  0x0000fffff24dcfd0 in mapping_open_pcm (m=0xaaaaaab28970, ss=0xaaaaaaae2fec, dev_id=0xaaaaaab12450 "0", exact_channels=true, mode=1, default_n_fragments=4,                   
    default_fragment_size_msec=25) at modules/alsa/alsa-mixer.c:4671
#3  0x0000fffff24ddf28 in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)       
    at modules/alsa/alsa-mixer.c:4891
#4  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871                                                                 
#5  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card",                                                        
    argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#6  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#7  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#8  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#9  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#10 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#11 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#12 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#13 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505,     
    ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#14 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#15 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
snd_pcm_t *pa_alsa_open_by_device_string(
        const char *device,
        char **dev,
        pa_sample_spec *ss,
        pa_channel_map* map,
        int mode,
        snd_pcm_uframes_t *period_size,
        snd_pcm_uframes_t *buffer_size,
        snd_pcm_uframes_t tsched_size,
        bool *use_mmap,
        bool *use_tsched,
        bool require_exact_channel_number) {

    int err;
    char *d;
    snd_pcm_t *pcm_handle;
    bool reformat = false;

    // 断言输入参数不为空
    pa_assert(device);
    pa_assert(ss);
    pa_assert(map);

    d = pa_xstrdup(device);

    for (;;) {
        pa_log_debug("Trying %s %s SND_PCM_NO_AUTO_FORMAT ...", d, reformat ? "without" : "with");

        // 尝试打开 ALSA 设备，此函数位于/usr/include/alsa/pcm.h
        if ((err = snd_pcm_open(&pcm_handle, d, mode,
                                SND_PCM_NONBLOCK|
                                SND_PCM_NO_AUTO_RESAMPLE|
                                SND_PCM_NO_AUTO_CHANNELS|
                                (reformat ? 0 : SND_PCM_NO_AUTO_FORMAT))) < 0) {
            pa_log_info("Error opening PCM device %s: %s", d, pa_alsa_strerror(err));
            goto fail;
        }

        pa_log_debug("Managed to open %s", d);

        // 尝试设置硬件参数
        if ((err = pa_alsa_set_hw_params(
                     pcm_handle,
                     ss,
                     period_size,
                     buffer_size,
                     tsched_size,
                     use_mmap,
                     use_tsched,
                     require_exact_channel_number)) < 0) {

            if (!reformat) {
                reformat = true;

                snd_pcm_close(pcm_handle);
                continue;
            }

            // 尝试通过插件打开设备
            if (!pa_startswith(d, "plug:") && !pa_startswith(d, "plughw:")) {
                char *t;

                t = pa_sprintf_malloc("plug:%s", d);
                pa_xfree(d);
                d = t;

                reformat = false;

                snd_pcm_close(pcm_handle);
                continue;
            }

            pa_log_info("Failed to set hardware parameters on %s: %s", d, pa_alsa_strerror(err));
            snd_pcm_close(pcm_handle);

            goto fail;
        }

        // 检查通道数是否超过最大支持数
        if (ss->channels > PA_CHANNELS_MAX) {
            pa_log("Device %s has %u channels, but PulseAudio supports only %u channels. Unable to use the device.",
                   d, ss->channels, PA_CHANNELS_MAX);
            snd_pcm_close(pcm_handle);
            goto fail;
        }

        // 根据是否传递了 dev 参数设置设备名
        if (dev)
            *dev = d;
        else
            pa_xfree(d);

        // 如果通道数不匹配，进行通道映射的扩展
        if (ss->channels != map->channels)
            pa_channel_map_init_extend(map, ss->channels, PA_CHANNEL_MAP_ALSA);

        return pcm_handle;
    }

fail:
    pa_xfree(d);

    return NULL;
}
```

![snd_pcm_open](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817170922.png)

#### mapping_query_hw_device

```c
(gdb) bt
#0  0x0000fffff24dd348 in mapping_query_hw_device (mapping=0xaaaaaab2c360, pcm=0xaaaaaacb4360) at modules/alsa/alsa-mixer.c:4719
#1  0x0000fffff24ddff4 in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4905
#2  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#3  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static void mapping_query_hw_device(pa_alsa_mapping *mapping, snd_pcm_t *pcm) {
    int r;
    snd_pcm_info_t* pcm_info;
    snd_pcm_info_alloca(&pcm_info);

    // 查询 PCM 设备信息
    r = snd_pcm_info(pcm, pcm_info);
    if (r < 0) {
        pa_log("映射 %s：snd_pcm_info() 失败 %s：", mapping->name, pa_alsa_strerror(r));
        return;
    }

    /* XXX: 不清楚 snd_pcm_info_get_device() 在设备不由硬件支持或由多个硬件设备支持的情况下的行为。
     * 然而，我们只在 HDMI 设备中使用 hw_device_index，对于这些设备，返回值应始终有效，
     * 因此这不应该是一个重大问题。 */
    // 获取硬件设备索引并设置给映射结构
    mapping->hw_device_index = snd_pcm_info_get_device(pcm_info);
}
```

#### mapping_paths_probe

`mapping_paths_probe`中会处理`/usr/share/pulseaudio/alsa-mixer/paths/`，见下文。

## /usr/share/pulseaudio/alsa-mixer/paths/

### mapping_paths_probe

```c
(gdb) bt
#0  0x0000fffff24db120 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4116
#1  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#2  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#3  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#4  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#5  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#6  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#7  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#8  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#9  0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#10 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#11 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#12 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#13 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/* 在指定映射中探测路径 */
static void mapping_paths_probe(pa_alsa_mapping *m, pa_alsa_profile *profile,
                                pa_alsa_direction_t direction, pa_hashmap *used_paths) {

    pa_alsa_path *p;
    void *state;
    snd_pcm_t *pcm_handle;
    pa_alsa_path_set *ps;
    snd_mixer_t *mixer_handle;

    if (direction == PA_ALSA_DIRECTION_OUTPUT) {
        if (m->output_path_set)
            return; /* 已经进行过探测 */
        m->output_path_set = ps = pa_alsa_path_set_new(m, direction, NULL); /* FIXME: 处理 paths_dir */
        pcm_handle = m->output_pcm;
    } else {
        if (m->input_path_set)
            return; /* 已经进行过探测 */
        m->input_path_set = ps = pa_alsa_path_set_new(m, direction, NULL); /* FIXME: 处理 paths_dir */
        pcm_handle = m->input_pcm;
    }

    if (!ps)
        return; /* 没有可用的路径 */

    pa_assert(pcm_handle);

    mixer_handle = pa_alsa_open_mixer_for_pcm(pcm_handle, NULL);
    if (!mixer_handle) {
        /* 无法打开混音器，移除所有路径 */
        pa_hashmap_remove_all(ps->paths);
        return;
    }

    /* 对于每个路径，进行探测并更新 */
    PA_HASHMAP_FOREACH(p, ps->paths, state) {
        if (p->autodetect_eld_device)
            p->eld_device = m->hw_device_index;

        if (pa_alsa_path_probe(p, m, mixer_handle, m->profile_set->ignore_dB) < 0)
            pa_hashmap_remove(ps->paths, p);
    }

    /* 精简路径集合 */
    path_set_condense(ps, mixer_handle);
    path_set_make_path_descriptions_unique(ps);

    if (mixer_handle)
        snd_mixer_close(mixer_handle);

    /* 将已使用的路径添加到哈希表中 */
    PA_HASHMAP_FOREACH(p, ps->paths, state)
        pa_hashmap_put(used_paths, p, p);

    pa_log_debug("可用的混音器路径（整理后）：");
    pa_alsa_path_set_dump(ps);
}
```

#### pa_alsa_path_set_new

```c
#0  0x0000fffff24d78c4 in pa_alsa_path_set_new (m=0xaaaaaab2c4b0, direction=PA_ALSA_DIRECTION_INPUT, paths_dir=0x0) at modules/alsa/alsa-mixer.c:3184
#1  0x0000fffff24db1a0 in mapping_paths_probe (m=0xaaaaaab2c4b0, profile=0xaaaaaab409f0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6bac0)
    at modules/alsa/alsa-mixer.c:4132
#2  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28270, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#3  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#4  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#5  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#6  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#7  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#8  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#9  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#10 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#11 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#12 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#13 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#14 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
ptype p
type = struct pa_alsa_path {
    pa_alsa_direction_t direction;           /**< ALSA 路径的方向，输入还是输出 */
    pa_device_port *port;                   /**< 设备端口指针 */
    char *name;                              /**< 路径的名称 */
    char *description_key;                   /**< 描述的键 */
    char *description;                       /**< 描述 */
    unsigned int priority;                   /**< 优先级 */
    _Bool autodetect_eld_device;             /**< 自动检测 ELD 设备标志 */
    int eld_device;                          /**< ELD 设备索引 */
    pa_proplist *proplist;                   /**< 属性列表指针 */
    _Bool probed : 1;                        /**< 是否已经探测 */
    _Bool supported : 1;                     /**< 是否支持 */
    _Bool has_mute : 1;                      /**< 是否有静音控制 */
    _Bool has_volume : 1;                    /**< 是否有音量控制 */
    _Bool has_dB : 1;                        /**< 是否支持分贝控制 */
    _Bool mute_during_activation : 1;        /**< 激活时是否静音 */
    _Bool has_req_any : 1;                   /**< 是否有 required-any 属性 */
    _Bool req_any_present : 1;               /**< 是否存在 required-any 属性 */
    _Bool customized : 1;                    /**< 是否自定义 */
    long min_volume;                         /**< 最小音量值 */
    long max_volume;                         /**< 最大音量值 */
    double min_dB;                           /**< 最小分贝值 */
    double max_dB;                           /**< 最大分贝值 */
    pa_alsa_element *last_element;           /**< 最后一个元素指针 */
    pa_alsa_option *last_option;             /**< 最后一个选项指针 */
    pa_alsa_setting *last_setting;           /**< 最后一个设置指针 */
    pa_alsa_jack *last_jack;                 /**< 最后一个插孔指针 */
    pa_alsa_element *elements;               /**< 元素链表指针 */
    pa_alsa_setting *settings;               /**< 设置链表指针 */
    pa_alsa_jack *jacks;                     /**< 插孔链表指针 */
} *
```

```c
/* 创建一个新的 ALSA 路径集 */
pa_alsa_path_set *pa_alsa_path_set_new(pa_alsa_mapping *m, pa_alsa_direction_t direction, const char *paths_dir) {
    pa_alsa_path_set *ps;
    char **pn = NULL, **en = NULL, **ie;
    pa_alsa_decibel_fix *db_fix;
    void *state, *state2;

    pa_assert(m);
    pa_assert(m->profile_set);
    pa_assert(m->profile_set->decibel_fixes);
    pa_assert(direction == PA_ALSA_DIRECTION_OUTPUT || direction == PA_ALSA_DIRECTION_INPUT);

    if (m->direction != PA_ALSA_DIRECTION_ANY && m->direction != direction)
        return NULL;

    ps = pa_xnew0(pa_alsa_path_set, 1);
    ps->direction = direction;
    ps->paths = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    if (direction == PA_ALSA_DIRECTION_OUTPUT)
        pn = m->output_path_names;
    else
        pn = m->input_path_names;

    if (pn) {
        char **in;

        for (in = pn; *in; in++) {
            pa_alsa_path *p = NULL;
            bool duplicate = false;
            char **kn;

            for (kn = pn; kn < in; kn++)
                if (pa_streq(*kn, *in)) {
                    duplicate = true;
                    break;
                }

            // 去重，算法时间复杂度为 O(n^2)，存在优化空间
            if (duplicate)
                continue;

            p = profile_set_get_path(m->profile_set, *in);

            if (p && p->direction != direction) {
                pa_log("配置错误：路径 %s 既被用作输入路径又被用作输出路径。", p->name);
                goto fail;
            }

            if (!p) {
                char *fn = pa_sprintf_malloc("%s.conf", *in);
                p = pa_alsa_path_new(m->profile_set, paths_dir, fn, direction);
                pa_xfree(fn);
                if (p)
                    profile_set_add_path(m->profile_set, p);
            }

            if (p)
                pa_hashmap_put(ps->paths, p, p);
        }

        goto finish;
    }

    if (direction == PA_ALSA_DIRECTION_OUTPUT)
        en = m->output_element;
    else
        en = m->input_element;

    if (!en)
        goto fail;

    for (ie = en; *ie; ie++) {
        char **je;
        pa_alsa_path *p;

        p = pa_alsa_path_synthesize(*ie, direction);

        /* 标记其他已传递的元素为 required-absent */
        for (je = en; *je; je++) {
            pa_alsa_element *e;

            if (je == ie)
                continue;

            e = pa_xnew0(pa_alsa_element, 1);
            e->path = p;
            e->alsa_name = pa_xstrdup(*je);
            e->direction = direction;
            e->required_absent = PA_ALSA_REQUIRED_ANY;
            e->volume_limit = -1;

            PA_LLIST_INSERT_AFTER(pa_alsa_element, p->elements, p->last_element, e);
            p->last_element = e;
        }

        pa_hashmap_put(ps->paths, *ie, p);
    }

finish:
    /* 将分贝修复分配给元素 */
    PA_HASHMAP_FOREACH(db_fix, m->profile_set->decibel_fixes, state) {
        pa_alsa_path *p;

        PA_HASHMAP_FOREACH(p, ps->paths, state2) {
            pa_alsa_element *e;

            PA_LLIST_FOREACH(e, p->elements) {
                if (e->volume_use != PA_ALSA_VOLUME_IGNORE && pa_streq(db_fix->name, e->alsa_name)) {
                    /* 包含 dB 修复的配置集可能会在元素之前被释放，所以我们必须复制 dB 修复对象。 */
                    e->db_fix = pa_xnewdup(pa_alsa_decibel_fix, db_fix, 1);
                    e->db_fix->profile_set = NULL;
                    e->db_fix->name = pa_xstrdup(db_fix->name);
                    e->db_fix->db_values = pa_xmemdup(db_fix->db_values, (db_fix->max_step - db_fix->min_step + 1) * sizeof(long));
                }
            }
        }
    }

    return ps;

fail:
    if (ps)
        pa_alsa_path_set_free(ps);

    return NULL;
}
```

```c
(gdb) p *pn@15
$48 =   {[0] = 0xaaaaaab2caf0 "analog-input-front-mic",
  [1] = 0xaaaaaab2cad0 "analog-input-rear-mic",
  [2] = 0xaaaaaab2cb10 "analog-input-internal-mic",
  [3] = 0xaaaaaab2cab0 "analog-input-dock-mic",
  [4] = 0xaaaaaab2c470 "analog-input",
  [5] = 0xaaaaaab2cb40 "analog-input-mic",
  [6] = 0xaaaaaab2cb60 "analog-input-linein",
  [7] = 0xaaaaaab2cb80 "analog-input-aux",
  [8] = 0xaaaaaab2cc30 "analog-input-video",
  [9] = 0xaaaaaab2cc50 "analog-input-tvtuner",
  [10] = 0xaaaaaab2cc70 "analog-input-fm",
  [11] = 0xaaaaaab2cc90 "analog-input-mic-line",
  [12] = 0xaaaaaab2ccb0 "analog-input-headphone-mic",
  [13] = 0xaaaaaab2cce0 "analog-input-headset-mic",
  [14] = 0x0}
(gdb) p *in@15                                                                                                                                                                    
$49 =   {[0] = 0xaaaaaab2caf0 "analog-input-front-mic",
  [1] = 0xaaaaaab2cad0 "analog-input-rear-mic",
  [2] = 0xaaaaaab2cb10 "analog-input-internal-mic",
  [3] = 0xaaaaaab2cab0 "analog-input-dock-mic",
  [4] = 0xaaaaaab2c470 "analog-input",
  [5] = 0xaaaaaab2cb40 "analog-input-mic",
  [6] = 0xaaaaaab2cb60 "analog-input-linein",
  [7] = 0xaaaaaab2cb80 "analog-input-aux",
  [8] = 0xaaaaaab2cc30 "analog-input-video",
  [9] = 0xaaaaaab2cc50 "analog-input-tvtuner",
  [10] = 0xaaaaaab2cc70 "analog-input-fm",
  [11] = 0xaaaaaab2cc90 "analog-input-mic-line",
  [12] = 0xaaaaaab2ccb0 "analog-input-headphone-mic",
  [13] = 0xaaaaaab2cce0 "analog-input-headset-mic",
  [14] = 0x0}
(gdb) p *kn@15                                                                                                                                                                    
$50 =   {[0] = 0xaaaaaab2caf0 "analog-input-front-mic",
  [1] = 0xaaaaaab2cad0 "analog-input-rear-mic",
  [2] = 0xaaaaaab2cb10 "analog-input-internal-mic",
  [3] = 0xaaaaaab2cab0 "analog-input-dock-mic",
  [4] = 0xaaaaaab2c470 "analog-input",
  [5] = 0xaaaaaab2cb40 "analog-input-mic",
  [6] = 0xaaaaaab2cb60 "analog-input-linein",
  [7] = 0xaaaaaab2cb80 "analog-input-aux",
  [8] = 0xaaaaaab2cc30 "analog-input-video",
  [9] = 0xaaaaaab2cc50 "analog-input-tvtuner",
  [10] = 0xaaaaaab2cc70 "analog-input-fm",
  [11] = 0xaaaaaab2cc90 "analog-input-mic-line",
  [12] = 0xaaaaaab2ccb0 "analog-input-headphone-mic",
  [13] = 0xaaaaaab2cce0 "analog-input-headset-mic",
  [14] = 0x0}
```

##### profile_set_get_path

```bash
(gdb) bt
#0  0x0000fffff7dbce30 in pa_hashmap_get (h=0xaaaaaab29cd0, key=0xaaaaaab2caf0) at pulsecore/hashmap.c:180                                                                        
#1  0x0000fffff24d7654 in profile_set_get_path (ps=0xaaaaaab28270, path_name=0xaaaaaab2caf0 "analog-input-front-mic") at modules/alsa/alsa-mixer.c:3163                           
#2  0x0000fffff24d7bc8 in pa_alsa_path_set_new (m=0xaaaaaab2c4b0, direction=PA_ALSA_DIRECTION_INPUT, paths_dir=0x0) at modules/alsa/alsa-mixer.c:3224                             
#3  0x0000fffff24db1a0 in mapping_paths_probe (m=0xaaaaaab2c4b0, profile=0xaaaaaab409f0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6bac0)                            
    at modules/alsa/alsa-mixer.c:4132
#4  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28270, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)       
    at modules/alsa/alsa-mixer.c:4933
#5  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871                                                                 
#6  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187        
#7  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#8  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#9  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#10 0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#11 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#12 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#13 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#14 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136        
#15 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#16 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/* 从配置文件中获取 ALSA 路径对象 */
static pa_alsa_path *profile_set_get_path(pa_alsa_profile_set *ps, const char *path_name) {
    pa_alsa_path *path;

    pa_assert(ps);            /**< 断言：确保 pa_alsa_profile_set 对象不为空 */
    pa_assert(path_name);     /**< 断言：确保路径名称不为空 */

    /* 从输出路径哈希映射中查找路径对象 */
    if ((path = pa_hashmap_get(ps->output_paths, path_name)))
        return path;

    /* 从输入路径哈希映射中查找路径对象 */
    return pa_hashmap_get(ps->input_paths, path_name);
}
```

###### pa_alsa_path_new

```bash
(gdb) bt
#0  0x0000fffff24d5214 in pa_alsa_path_new (ps=0xaaaaaab28270, paths_dir=0x0, fname=0xaaaaaacb0c90 "analog-input-front-mic.conf", direction=PA_ALSA_DIRECTION_INPUT)
    at modules/alsa/alsa-mixer.c:2665
#1  0x0000fffff24d7c60 in pa_alsa_path_set_new (m=0xaaaaaab2c4b0, direction=PA_ALSA_DIRECTION_INPUT, paths_dir=0x0) at modules/alsa/alsa-mixer.c:3233
#2  0x0000fffff24db1a0 in mapping_paths_probe (m=0xaaaaaab2c4b0, profile=0xaaaaaab409f0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6bac0)
    at modules/alsa/alsa-mixer.c:4132
#3  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28270, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#4  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#5  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#6  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#7  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#8  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#9  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#10 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#11 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#12 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#13 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#14 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#15 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/**
 * 创建新的 ALSA 路径对象。
 *
 * @param ps               pa_alsa_profile_set 对象
 * @param paths_dir        路径配置文件所在的目录
 * @param fname            路径配置文件的文件名
 * @param direction        路径方向（输入或输出）
 *
 * @return                 新创建的 pa_alsa_path 对象，如果出错则返回 NULL
 */
pa_alsa_path* pa_alsa_path_new(const pa_alsa_profile_set *ps, const char *paths_dir, const char *fname, pa_alsa_direction_t direction) {
    pa_alsa_path *p;
    char *fn;
    char *cust_fn = NULL;
    int r;
    const char *n;
    bool mute_during_activation = false;

    pa_config_item items[] = {
        /* [General] */
        { "priority",            pa_config_parse_unsigned,          NULL, "General" },
        { "description-key",     pa_config_parse_string,            NULL, "General" },
        { "description",         pa_config_parse_string,            NULL, "General" },
        { "mute-during-activation", pa_config_parse_bool,           NULL, "General" },
        { "eld-device",          parse_eld_device,                  NULL, "General" },
        /* ... 其他选项 ... */
        { NULL, NULL, NULL, NULL }
    };

    pa_assert(fname);

    p = pa_xnew0(pa_alsa_path, 1);
    n = pa_path_get_filename(fname);
    p->name = pa_xstrndup(n, strcspn(n, "."));
    p->proplist = pa_proplist_new();
    p->direction = direction;
    p->eld_device = -1;

    items[0].data = &p->priority;
    items[1].data = &p->description_key;
    items[2].data = &p->description;
    items[3].data = &mute_during_activation;

    if (!paths_dir)
        paths_dir = get_default_paths_dir();

    /* 如果 fname 在自定义文件夹中，则加载自定义文件名 */
    if (ps && ps->cust_folder)
        cust_fn = pa_alsa_get_cust_filename(ps->cust_paths, fname);

    if (cust_fn) {
        fn = pa_maybe_prefix_path(cust_fn, paths_dir);
        p->customized = true;
        pa_xfree(cust_fn);
    } else
        fn = pa_maybe_prefix_path(fname, paths_dir);

    pa_log_info("Loading path config: %s,%s,%s,%s", fn, fname, n, p->name);

    r = pa_config_parse(fn, NULL, items, p->proplist, false, p);
    pa_xfree(fn);

    if (r < 0)
        goto fail;

    p->mute_during_activation = mute_during_activation;

    if (path_verify(p) < 0)
        goto fail;

    return p;

fail:
    pa_alsa_path_free(p);
    return NULL;
}
```

这个函数中的主要函数`pa_config_parse`解读参见上方。

![20230817180909](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817180909.png)
![20230817181046](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817181046.png)

```c
0x0000fffff24d7c60 in pa_alsa_path_set_new (m=0xaaaaaab2c4b0, direction=PA_ALSA_DIRECTION_INPUT, paths_dir=0x0) at modules/alsa/alsa-mixer.c:3233                                 
3233                    p = pa_alsa_path_new(m->profile_set, paths_dir, fn, direction);
Value returned is $57 = (pa_alsa_path *) 0xaaaaaacb3a80
(gdb) p *p                                                                                                                                                                        
Cannot access memory at address 0x0
(gdb) n
3234                    pa_xfree(fn);
(gdb) p *p
$58 = {
  direction = PA_ALSA_DIRECTION_INPUT,
  port = 0x0,
  name = 0xaaaaaacb3c60 "analog-input-front-mic",
  description_key = 0xaaaaaac6ab50 "analog-input-microphone-front",
  description = 0xaaaaaacb4bd0 "前麦克风",
  priority = 85,
  autodetect_eld_device = false,
  eld_device = -1,
  proplist = 0xaaaaaacb46f0,
  probed = false,
  supported = false,
  has_mute = false,
  has_volume = false,
  has_dB = false,
  mute_during_activation = false,
  has_req_any = true,
  req_any_present = false,
  customized = false,
  min_volume = 0,
  max_volume = 0,
  min_dB = 0,
--Type <RET> for more, q to quit, c to continue without paging--
  max_dB = 0, 
  last_element = 0xaaaaaacc2bf0, 
  last_option = 0xaaaaaacc34b0, 
  last_setting = 0x0, 
  last_jack = 0xaaaaaacb36f0, 
  elements = 0xaaaaaacb5f80, 
  settings = 0x0, 
  jacks = 0xaaaaaacb3c80
}
```

##### profile_set_add_path

```c
(gdb) n
3235                    if (p)
(gdb)
3236                        profile_set_add_path(m->profile_set, p);
(gdb) s
profile_set_add_path (ps=0xaaaaaab28270, path=0xaaaaaacb3a80) at modules/alsa/alsa-mixer.c:3167                                                                                   
3167        pa_assert(ps);
(gdb) bt
#0  0x0000fffff24d7674 in profile_set_add_path (ps=0xaaaaaab28270, path=0xaaaaaacb3a80) at modules/alsa/alsa-mixer.c:3167                                                         
#1  0x0000fffff24d7c88 in pa_alsa_path_set_new (m=0xaaaaaab2c4b0, direction=PA_ALSA_DIRECTION_INPUT, paths_dir=0x0) at modules/alsa/alsa-mixer.c:3236                             
#2  0x0000fffff24db1a0 in mapping_paths_probe (m=0xaaaaaab2c4b0, profile=0xaaaaaab409f0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6bac0)                            
    at modules/alsa/alsa-mixer.c:4132
#3  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28270, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)       
    at modules/alsa/alsa-mixer.c:4933
#4  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871                                                                 
#5  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187        
#6  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#7  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#8  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#9  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#10 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#11 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#12 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#13 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136        
#14 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#15 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/**
 * 向 ALSA 配置文件集合中添加路径对象。
 *
 * @param ps     pa_alsa_profile_set 对象
 * @param path   要添加的 pa_alsa_path 对象
 */
static void profile_set_add_path(pa_alsa_profile_set *ps, pa_alsa_path *path) {
    pa_assert(ps);
    pa_assert(path);

    switch (path->direction) {
        case PA_ALSA_DIRECTION_OUTPUT:
            pa_assert_se(pa_hashmap_put(ps->output_paths, path->name, path) >= 0);
            break;

        case PA_ALSA_DIRECTION_INPUT:
            pa_assert_se(pa_hashmap_put(ps->input_paths, path->name, path) >= 0);
            break;

        default:
            pa_assert_not_reached();
    }
}
```

##### pa_alsa_path_synthesize

```c
typedef enum pa_alsa_direction {
    PA_ALSA_DIRECTION_ANY,    /* 任意方向 */
    PA_ALSA_DIRECTION_OUTPUT, /* 输出方向，从设备流向外部 */
    PA_ALSA_DIRECTION_INPUT   /* 输入方向，从外部流向设备 */
} pa_alsa_direction_t;        /* ALSA方向的枚举类型 */
```

src/modules/alsa/alsa-mixer.c：3247-3253：

```c
if (direction == PA_ALSA_DIRECTION_OUTPUT)
        en = m->output_element;
    else
        en = m->input_element;

    if (!en)
        goto fail;
```

3247-3253行决定`pa_alsa_path_synthesize`函数是否被调用：

src/modules/alsa/alsa-mixer.c：3259
src/modules/alsa/alsa-mixer.c：2765

```c
/**
 * 合成一个虚拟的 ALSA 路径对象。
 *
 * @param element    要合成的元素名称
 * @param direction  路径的方向（输入或输出）
 *
 * @return 生成的 pa_alsa_path 对象
 */
pa_alsa_path *pa_alsa_path_synthesize(const char *element, pa_alsa_direction_t direction) {
    pa_alsa_path *p;
    pa_alsa_element *e;

    pa_assert(element);

    // 创建新的 pa_alsa_path 对象
    p = pa_xnew0(pa_alsa_path, 1);
    p->name = pa_xstrdup(element);
    p->direction = direction;
    p->proplist = pa_proplist_new();

    // 创建新的 pa_alsa_element 对象，并添加到路径中
    e = pa_xnew0(pa_alsa_element, 1);
    e->path = p;
    e->alsa_name = pa_xstrdup(element);
    e->direction = direction;
    e->volume_limit = -1;

    e->switch_use = PA_ALSA_SWITCH_MUTE;
    e->volume_use = PA_ALSA_VOLUME_MERGE;

    PA_LLIST_PREPEND(pa_alsa_element, p->elements, e);
    p->last_element = e;
    return p;
}
```

#### pa_alsa_open_mixer_for_pcm

```c
(gdb) s
pa_alsa_open_mixer_for_pcm (pcm=0xaaaaaacb4360, ctl_device=0x0) at modules/alsa/alsa-util.c:1638                                                                                  
warning: Source file is more recent than executable.
1638    snd_mixer_t *pa_alsa_open_mixer_for_pcm(snd_pcm_t *pcm, char **ctl_device) {
(gdb) bt
#0  0x0000fffff24c2dec in pa_alsa_open_mixer_for_pcm (pcm=0xaaaaaacb4360, ctl_device=0x0) at modules/alsa/alsa-util.c:1638                                                        
#1  0x0000fffff24db238 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)                            
    at modules/alsa/alsa-mixer.c:4141
#2  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)       
    at modules/alsa/alsa-mixer.c:4933
#3  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871                                                                 
#4  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187        
#5  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333                                                                  
#6  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422                                                                 
#7  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464                                                               
#8  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481                
#9  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789                                                                  
#10 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187              
#11 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437                        
#12 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136        
#13 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176        
#14 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
snd_mixer_t *pa_alsa_open_mixer_for_pcm(snd_pcm_t *pcm, char **ctl_device) {
    int err;
    snd_mixer_t *m;
    const char *dev;
    snd_pcm_info_t* info;
    snd_pcm_info_alloca(&info);

    pa_assert(pcm);

    // 尝试通过名称打开混音器
    if ((err = snd_mixer_open(&m, 0)) < 0) {
        pa_log("Error opening mixer: %s", pa_alsa_strerror(err));
        return NULL;
    }

    // 首先，尝试使用名称
    if ((dev = snd_pcm_name(pcm)))
        if (prepare_mixer(m, dev) >= 0) {
            if (ctl_device)
                *ctl_device = pa_xstrdup(dev);

            return m;
        }

    // 然后，尝试使用卡索引
    if (snd_pcm_info(pcm, info) >= 0) {
        char *md;
        int card_idx;

        if ((card_idx = snd_pcm_info_get_card(info)) >= 0) {

            md = pa_sprintf_malloc("hw:%i", card_idx);

            if (!dev || !pa_streq(dev, md))
                if (prepare_mixer(m, md) >= 0) {

                    if (ctl_device)
                        *ctl_device = md;
                    else
                        pa_xfree(md);

                    return m;
                }

            pa_xfree(md);
        }
    }

    snd_mixer_close(m);
    return NULL;
}
```

![20230817184204](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817184204.png)
![20230817184248](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817184248.png)
![20230817184347](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817184347.png)

###### prepare_mixer

```bash
1654            if (prepare_mixer(m, dev) >= 0) {
(gdb) s
prepare_mixer (mixer=0xaaaaaad5f180, dev=0xaaaaaacb40d0 "hw:0") at modules/alsa/alsa-util.c:1569
1569    static int prepare_mixer(snd_mixer_t *mixer, const char *dev) {
(gdb) info args 
mixer = 0xaaaaaad5f180
dev = 0xaaaaaacb40d0 "hw:0"
```

```c
/* 准备混音器 */
static int prepare_mixer(snd_mixer_t *mixer, const char *dev) {
    int err;
    snd_mixer_class_t *class;

    pa_assert(mixer); /* 断言 mixer 不为 NULL */
    pa_assert(dev); /* 断言 dev 不为 NULL */

    /* 尝试将混音器附加到指定的设备 */
    if ((err = snd_mixer_attach(mixer, dev)) < 0) {
        pa_log_info("无法附加到混音器 %s: %s", dev, pa_alsa_strerror(err));
        return -1;
    }

    /* 分配混音器类并设置相关参数 */
    if (snd_mixer_class_malloc(&class)) {
        pa_log_info("为 %s 分配混音器类失败", dev);
        return -1;
    }
    snd_mixer_class_set_event(class, mixer_class_event);
    snd_mixer_class_set_compare(class, mixer_class_compare);
    if ((err = snd_mixer_class_register(class, mixer)) < 0) {
        pa_log_info("无法为 %s 注册混音器类: %s", dev, pa_alsa_strerror(err));
        snd_mixer_class_free(class);
        return -1;
    }
    /* 从这里开始，混音器类将由 alsa 在 snd_mixer_close/free 时释放。 */

    /* 注册混音器 */
    if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0) {
        pa_log_warn("无法注册混音器: %s", pa_alsa_strerror(err));
        return -1;
    }

    /* 加载混音器 */
    if ((err = snd_mixer_load(mixer)) < 0) {
        pa_log_warn("无法加载混音器: %s", pa_alsa_strerror(err));
        return -1;
    }

    pa_log_info("成功附加到混音器 '%s'", dev);
    return 0;
}
```

#### pa_alsa_path_probe

```bash
(gdb) bt
#0  0x0000fffff24d6144 in pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2922
#1  0x0000fffff24db2c0 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4152
#2  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#3  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#4  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#5  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#6  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#7  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#8  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#9  0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#10 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#11 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#12 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#13 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#14 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
int pa_alsa_path_probe(pa_alsa_path *p, pa_alsa_mapping *mapping, snd_mixer_t *m, bool ignore_dB) {
    pa_alsa_element *e;
    pa_alsa_jack *j;
    double min_dB[PA_CHANNEL_POSITION_MAX], max_dB[PA_CHANNEL_POSITION_MAX];
    pa_channel_position_t t;
    pa_channel_position_mask_t path_volume_channels = 0;

    pa_assert(p);
    pa_assert(m);

    // 如果路径已经被探测过，返回已知的支持状态
    if (p->probed)
        return p->supported ? 0 : -1;
    p->probed = true;

    pa_zero(min_dB);
    pa_zero(max_dB);

    pa_log_debug("Probing path '%s'", p->name);

    // 为路径中的每个插孔进行探测
    PA_LLIST_FOREACH(j, p->jacks) {
        if (jack_probe(j, mapping, m) < 0) {
            p->supported = false;
            pa_log_debug("Probe of jack '%s' failed.", j->alsa_name);
            return -1;
        }
        pa_log_debug("Probe of jack '%s' succeeded (%s)", j->alsa_name, j->has_control ? "found!" : "not found");
    }

    // 为路径中的每个元素进行探测
    PA_LLIST_FOREACH(e, p->elements) {
        if (element_probe(e, m) < 0) {
            p->supported = false;
            pa_log_debug("Probe of element '%s' failed.", e->alsa_name);
            return -1;
        }
        pa_log_debug("Probe of element '%s' succeeded (volume=%d, switch=%d, enumeration=%d).", e->alsa_name, e->volume_use, e->switch_use, e->enumeration_use);

        // 如果忽略 dB，将标志设为 false
        if (ignore_dB)
            e->has_dB = false;

        // 处理合并音量的情况
        if (e->volume_use == PA_ALSA_VOLUME_MERGE) {
            // 如果路径中没有音量元素，设置音量范围
            if (!p->has_volume) {
                p->min_volume = e->min_volume;
                p->max_volume = e->max_volume;
            }

            // 如果元素具有 dB 信息
            if (e->has_dB) {
                if (!p->has_volume) {
                    for (t = 0; t < PA_CHANNEL_POSITION_MAX; t++) {
                        if (PA_CHANNEL_POSITION_MASK(t) & e->merged_mask) {
                            min_dB[t] = e->min_dB;
                            max_dB[t] = e->max_dB;
                            path_volume_channels |= PA_CHANNEL_POSITION_MASK(t);
                        }
                    }
                    p->has_dB = true;
                } else {
                    if (p->has_dB) {
                        for (t = 0; t < PA_CHANNEL_POSITION_MAX; t++) {
                            if (PA_CHANNEL_POSITION_MASK(t) & e->merged_mask) {
                                min_dB[t] += e->min_dB;
                                max_dB[t] += e->max_dB;
                                path_volume_channels |= PA_CHANNEL_POSITION_MASK(t);
                            }
                        }
                    } else {
                        // 前面有一个不能进行 dB 音量的元素
                        e->volume_use = PA_ALSA_VOLUME_ZERO;
                        pa_log_info("Zeroing volume of '%s' on path '%s'", e->alsa_name, p->name);
                    }
                }
            } else if (p->has_volume) {
                // 无法使用音量，将其标志为忽略
                e->volume_use = PA_ALSA_VOLUME_IGNORE;
                pa_log_info("Ignoring volume of '%s' on path '%s' (missing dB info)", e->alsa_name, p->name);
            }
            p->has_volume = true;
        }

        if (e->switch_use == PA_ALSA_SWITCH_MUTE)
            p->has_mute = true;
    }

    // 如果需要，检查 required-any 元素是否存在
    if (p->has_req_any && !p->req_any_present) {
        p->supported = false;
        pa_log_debug("Skipping path '%s', none of required-any elements preset.", p->name);
        return -1;
    }

    path_drop_unsupported(p);
    path_make_options_unique(p);
    path_create_settings(p);

    p->supported = true;

    p->min_dB = INFINITY;
    p->max_dB = -INFINITY;

    for (t = 0; t < PA_CHANNEL_POSITION_MAX; t++) {
        if (path_volume_channels & PA_CHANNEL_POSITION_MASK(t)) {
            if (p->min_dB > min_dB[t])
                p->min_dB = min_dB[t];

            if (p->max_dB < max_dB[t])
                p->max_dB = max_dB[t];
        }
    }

    return 0;
}
```

##### jack_probe

```bash
(gdb) bt
#0  0x0000fffff24d1b38 in jack_probe (j=0xaaaaaacb4040, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180) at modules/alsa/alsa-mixer.c:1849
#1  0x0000fffff24d62e4 in pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2942
#2  0x0000fffff24db2c0 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4152
#3  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#4  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#5  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#6  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#7  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#8  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#9  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#10 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#11 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#12 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#13 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#14 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#15 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/* 检测是否存在控制选项 */
static int jack_probe(pa_alsa_jack *j, pa_alsa_mapping *mapping, snd_mixer_t *m) {
    bool has_control;

    pa_assert(j);
    pa_assert(j->path);

    /* 如果将 pcm 附加到名称中 */
    if (j->append_pcm_to_name) {
        char *new_name;

        if (!mapping) {
            /* 这也可以作为断言，因为这不应该发生。在撰写本文时，当 module-alsa-sink/source 合成路径时，映射只能为 NULL，而那些合成的路径永远不会有任何插孔，因此不应该调用 jack_probe() 以 NULL 映射。 */
            pa_log("插孔 %s: append_pcm_to_name 已设置，但映射为 NULL。无法使用此插孔。", j->name);
            return -1;
        }

        new_name = pa_sprintf_malloc("%s,pcm=%i Jack", j->name, mapping->hw_device_index);
        pa_xfree(j->alsa_name);
        j->alsa_name = new_name;
        j->append_pcm_to_name = false;
    }

    /* 查找是否有控制选项 */
    has_control = pa_alsa_mixer_find(m, j->alsa_name, 0) != NULL;
    /**
    内部基于ucm设备的控制选项,故此函数啥都没干
    **/
    pa_alsa_jack_set_has_control(j, has_control);

    /* 如果存在控制选项 */
    if (j->has_control) {
        if (j->required_absent != PA_ALSA_REQUIRED_IGNORE)
            return -1;
        if (j->required_any != PA_ALSA_REQUIRED_IGNORE)
            j->path->req_any_present = true;
    } else {
        /* 如果不存在控制选项 */
        if (j->required != PA_ALSA_REQUIRED_IGNORE)
            return -1;
    }

    return 0;
}
```

```bash
(gdb) p *j
$71 = {
  path = 0xaaaaaacb3dd0, 
  next = 0xaaaaaacb3a20, 
  prev = 0x0, 
  name = 0xaaaaaacb3f20 "Front Mic", 
  alsa_name = 0xaaaaaacb3fd0 "Front Mic Jack", 
  has_control = false, 
  plugged_in = false, 
  melem = 0x0, 
  state_unplugged = PA_AVAILABLE_NO, 
  state_plugged = PA_AVAILABLE_YES, 
  required = PA_ALSA_REQUIRED_IGNORE, 
  required_any = PA_ALSA_REQUIRED_ANY, 
  required_absent = PA_ALSA_REQUIRED_IGNORE, 
  ucm_devices = 0xaaaaaac6e120, 
  ucm_hw_mute_devices = 0xaaaaaac6c870, 
  append_pcm_to_name = false
}
```

```c
(gdb) p *j->path
$72 = {
  direction = PA_ALSA_DIRECTION_INPUT,
  port = 0x0,
  name = 0xaaaaaacb3fb0 "analog-input-front-mic",
  description_key = 0xaaaaaac6aa00 "analog-input-microphone-front",
  description = 0xaaaaaacb4f20 "前麦克风",
  priority = 85,
  autodetect_eld_device = false,
  eld_device = -1,
  proplist = 0xaaaaaacb4a40,
  probed = true,
  supported = false,
  has_mute = false,
  has_volume = false,
  has_dB = false,
  mute_during_activation = false,
  has_req_any = true,
  req_any_present = false,
  customized = false,
  min_volume = 0,
  max_volume = 0,
  min_dB = 0,
  max_dB = 0,
  last_element = 0xaaaaaacc2f40,
  last_option = 0xaaaaaacc3800,
--Type <RET> for more, q to quit, c to continue without paging--
```

在vim命令模式下使用`:echo expand('%:p')`显示当前文件的绝对路径：
![20230817190632](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230817190632.png)

```c
(gdb) fin
Run till exit from #0  jack_probe (j=0xaaaaaacb4040, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180) at modules/alsa/alsa-mixer.c:1850
pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2942
2942            if (jack_probe(j, mapping, m) < 0) {
Value returned is $73 = 0
(gdb) until 2951
(2323.588| 696.589) D: [pulseaudio][modules/alsa/alsa-mixer.c:2947 pa_alsa_path_probe()] Probe of jack 'Front Mic Jack' succeeded (found!)
(2323.588|   0.000) D: [pulseaudio][modules/alsa/alsa-mixer.c:2947 pa_alsa_path_probe()] Probe of jack 'Front Mic - Input Jack' succeeded (not found)
(2323.588|   0.000) D: [pulseaudio][modules/alsa/alsa-mixer.c:2947 pa_alsa_path_probe()] Probe of jack 'Front Mic Phantom Jack' succeeded (not found)
(2323.588|   0.000) D: [pulseaudio][modules/alsa/alsa-mixer.c:2947 pa_alsa_path_probe()] Probe of jack 'Front Line Out Front Jack' succeeded (not found)
pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2951
2951            if (element_probe(e, m) < 0) {
```

##### element_probe

```bash
(gdb) bt
#0  0x0000fffff24d1654 in element_probe (e=0xaaaaaacb62d0, m=0xaaaaaad5f180) at modules/alsa/alsa-mixer.c:1760
#1  0x0000fffff24d63c0 in pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2951
#2  0x0000fffff24db2c0 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4152
#3  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#4  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#5  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#6  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#7  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#8  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#9  0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#10 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#11 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#12 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#13 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#14 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#15 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
/* 探测音频元素 */
static int element_probe(pa_alsa_element *e, snd_mixer_t *m) {
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *me;

    pa_assert(m);
    pa_assert(e);
    pa_assert(e->path);

    SELEM_INIT(sid, e->alsa_name);

    // 用于在混音器中查找特定的音频元素，以便进行音量、开关等控制
    if (!(me = snd_mixer_find_selem(m, sid))) {

        if (e->required != PA_ALSA_REQUIRED_IGNORE)
            return -1;

        e->switch_use = PA_ALSA_SWITCH_IGNORE;
        e->volume_use = PA_ALSA_VOLUME_IGNORE;
        e->enumeration_use = PA_ALSA_ENUMERATION_IGNORE;

        return 0;
    }

    if (e->switch_use != PA_ALSA_SWITCH_IGNORE) {
        if (e->direction == PA_ALSA_DIRECTION_OUTPUT) {

            // 检查给定的音频元素是否具有可用的播放开关
            if (!snd_mixer_selem_has_playback_switch(me)) {
                // snd_mixer_selem_has_capture_switch 检查给定的音频元素是否具有可用的捕获开关（录音开关）
                if (e->direction_try_other && snd_mixer_selem_has_capture_switch(me))
                    e->direction = PA_ALSA_DIRECTION_INPUT;
                else
                    e->switch_use = PA_ALSA_SWITCH_IGNORE;
            }

        } else {

            if (!snd_mixer_selem_has_capture_switch(me)) {
                if (e->direction_try_other && snd_mixer_selem_has_playback_switch(me))
                    e->direction = PA_ALSA_DIRECTION_OUTPUT;
                else
                    e->switch_use = PA_ALSA_SWITCH_IGNORE;
            }
        }

        if (e->switch_use != PA_ALSA_SWITCH_IGNORE)
            e->direction_try_other = false;
    }

    if (!element_probe_volume(e, me))
        e->volume_use = PA_ALSA_VOLUME_IGNORE;

    if (e->switch_use == PA_ALSA_SWITCH_SELECT) {
        pa_alsa_option *o;

        PA_LLIST_FOREACH(o, e->options)
            o->alsa_idx = pa_streq(o->alsa_name, "on") ? 1 : 0;
    } else if (e->enumeration_use == PA_ALSA_ENUMERATION_SELECT) {
        int n;
        pa_alsa_option *o;
        /**
        snd_mixer_selem_get_enum_items
        函数用于获取音频混音器元素（也称为音量控制器）的枚举选项数量。
        在音频控制中，有些元素可能是离散的，即它们只能在一组预定义选项中进行选择，例如音频输入源或音效设置。
        这个函数可以用来查询元素的枚举选项数量，以便在对应的设置中做出适当的选择。
        **/
        if ((n = snd_mixer_selem_get_enum_items(me)) < 0) {
            pa_log("snd_mixer_selem_get_enum_items() 失败：%s", pa_alsa_strerror(n));
            return -1;
        }

        PA_LLIST_FOREACH(o, e->options) {
            int i;

            for (i = 0; i < n; i++) {
                char buf[128];

                // snd_mixer_selem_get_enum_item_name 用于获取混音器元素的不同枚举选项的可读名称，以便在用户界面中显示给用户选择
                if (snd_mixer_selem_get_enum_item_name(me, i, sizeof(buf), buf) < 0)
                    continue;

                if (!pa_streq(buf, o->alsa_name))
                    continue;

                o->alsa_idx = i;
            }
        }
    }

    if (check_required(e, me) < 0)
        return -1;

    return 0;
}
```

```c
ptype e
type = struct pa_alsa_element {
    pa_alsa_path *path;                  /* 所属的音频路径 */
    pa_alsa_element *next;               /* 下一个音频元素 */
    pa_alsa_element *prev;               /* 上一个音频元素 */
    char *alsa_name;                     /* ALSA元素名 */
    pa_alsa_direction_t direction;       /* 方向 */
    pa_alsa_switch_use_t switch_use;     /* 开关使用方式 */
    pa_alsa_volume_use_t volume_use;     /* 音量使用方式 */
    pa_alsa_enumeration_use_t enumeration_use; /* 枚举使用方式 */
    pa_alsa_required_t required;         /* 必须存在 */
    pa_alsa_required_t required_any;     /* 任意必须存在 */
    pa_alsa_required_t required_absent;  /* 必须缺失 */
    long constant_volume;                /* 固定音量 */
    unsigned int override_map;           /* 覆盖映射 */
    _Bool direction_try_other : 1;       /* 尝试其他方向 */
    _Bool has_dB : 1;                    /* 是否有分贝控制 */
    long min_volume;                     /* 最小音量 */
    long max_volume;                     /* 最大音量 */
    long volume_limit;                   /* 音量限制 */
    double min_dB;                       /* 最小分贝 */
    double max_dB;                       /* 最大分贝 */
    pa_channel_position_mask_t masks[32][8]; /* 通道掩码 */
    unsigned int n_channels;             /* 通道数 */
    pa_channel_position_mask_t merged_mask; /* 合并后的通道掩码 */
    pa_alsa_option *options;             /* 选项 */
    pa_alsa_decibel_fix *db_fix;         /* 分贝修正 */
} *
```

```c
(gdb) p *e
$78 = {         
  path = 0xaaaaaacb3dd0,                                        
  next = 0xaaaaaacb6b70,
  prev = 0x0,
  alsa_name = 0xaaaaaacb3db0 "Capture",
  direction = PA_ALSA_DIRECTION_INPUT,
  switch_use = PA_ALSA_SWITCH_MUTE,
  volume_use = PA_ALSA_VOLUME_MERGE,
  enumeration_use = PA_ALSA_ENUMERATION_IGNORE,
  required = PA_ALSA_REQUIRED_IGNORE,
  required_any = PA_ALSA_REQUIRED_IGNORE,
  required_absent = PA_ALSA_REQUIRED_IGNORE,
  constant_volume = 0,
  override_map = 3,
  direction_try_other = false,
  has_dB = false,
  min_volume = 0,
  max_volume = 0,
  volume_limit = -1,
  min_dB = 0,
  max_dB = 0,
  masks =     {[0] =       {[0] = 2251799813685247,
      [1] = 316659348800802,
      [2] = 0,
      [3] = 0,
      [4] = 0,
      [5] = 0,
      [6] = 0,
      [7] = 0},
    [1] =       {[0] = 0,
      [1] = 633318697601604,
      [2] = 0,
      [3] = 0,
      [4] = 0,
      [5] = 0,
      [6] = 0,
      [7] = 0},
    [2] =       {[0] = 0,
      [1] = 0,
      [2] = 0,
      [3] = 0,
      [4] = 0,
      [5] = 0,
      [6] = 0,
      [7] = 0} <repeats 30 times>},
  n_channels = 0,
  merged_mask = 0,
  options = 0x0,
  db_fix = 0x0
}
```

```c
(gdb) p *m
$79 = {
  slaves = {
    next = 0xaaaaaad5f1f8, 
    prev = 0xaaaaaad5f1f8
  }, 
  classes = {
    next = 0xaaaaaad5f520, 
    prev = 0xaaaaaad5fe20
  }, 
  elems = {
    next = 0xaaaaaad763e8, 
    prev = 0xaaaaaad82028
  }, 
  pelems = 0xaaaaaacad310, 
  count = 29, 
  alloc = 32, 
  events = 65, 
  callback = 0x0, 
  callback_private = 0x0, 
  compare = 0xfffff2563e10 <snd_mixer_compare_default>
}
```

###### element_probe_volume 

```c
(gdb) bt
#0  0x0000fffff24d06bc in element_probe_volume (e=0xaaaaaacb62d0, me=0xaaaaaad7f6b0) at modules/alsa/alsa-mixer.c:1543
#1  0x0000fffff24d1944 in element_probe (e=0xaaaaaacb62d0, m=0xaaaaaad5f180) at modules/alsa/alsa-mixer.c:1806
#2  0x0000fffff24d63c0 in pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2951
#3  0x0000fffff24db2c0 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4152
#4  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#5  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#6  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#7  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#8  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#9  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#10 0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#11 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#12 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#13 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#14 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#15 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#16 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static bool element_probe_volume(pa_alsa_element *e, snd_mixer_elem_t *me) {
    // 初始化变量
    long min_dB = 0, max_dB = 0;
    int r;
    bool is_mono;
    pa_channel_position_t p;

    // 根据方向检查音量支持情况
    if (e->direction == PA_ALSA_DIRECTION_OUTPUT) {
        // 检查指定的混音器元素（音频通道或控制）是否支持播放音量控制
        if (!snd_mixer_selem_has_playback_volume(me)) {
            if (e->direction_try_other && snd_mixer_selem_has_capture_volume(me))
                e->direction = PA_ALSA_DIRECTION_INPUT;
            else
                return false;
        }
    } else {
        if (!snd_mixer_selem_has_capture_volume(me)) {
            if (e->direction_try_other && snd_mixer_selem_has_playback_volume(me))
                e->direction = PA_ALSA_DIRECTION_OUTPUT;
            else
                return false;
        }
    }

    e->direction_try_other = false;

    // 获取指定的混音器元素（音频通道或控制）的播放音量范围
    if (e->direction == PA_ALSA_DIRECTION_OUTPUT)
        r = snd_mixer_selem_get_playback_volume_range(me, &e->min_volume, &e->max_volume);
    else
        r = snd_mixer_selem_get_capture_volume_range(me, &e->min_volume, &e->max_volume);

    if (r < 0) {
        pa_log_warn("Failed to get volume range of %s: %s", e->alsa_name, pa_alsa_strerror(r));
        return false;
    }

    // 检查音量范围的合法性
    if (e->min_volume >= e->max_volume) {
        pa_log_warn("Your kernel driver is broken: it reports a volume range from %li to %li which makes no sense.",
                    e->min_volume, e->max_volume);
        return false;
    }
    if (e->volume_use == PA_ALSA_VOLUME_CONSTANT && (e->min_volume > e->constant_volume || e->max_volume < e->constant_volume)) {
        pa_log_warn("Constant volume %li configured for element %s, but the available range is from %li to %li.",
                    e->constant_volume, e->alsa_name, e->min_volume, e->max_volume);
        return false;
    }

    // 检查是否需要修正 dB 范围
    if (e->db_fix && ((e->min_volume > e->db_fix->min_step) || (e->max_volume < e->db_fix->max_step))) {
        pa_log_warn("The step range of the decibel fix for element %s (%li-%li) doesn't fit to the "
                    "real hardware range (%li-%li). Disabling the decibel fix.", e->alsa_name,
                    e->db_fix->min_step, e->db_fix->max_step, e->min_volume, e->max_volume);

        decibel_fix_free(e->db_fix);
        e->db_fix = NULL;
    }

    // 确定是否支持 dB 范围，如果支持则获取 dB 范围
    if (e->db_fix) {
        e->has_dB = true;
        e->min_volume = e->db_fix->min_step;
        e->max_volume = e->db_fix->max_step;
        min_dB = e->db_fix->db_values[0];
        max_dB = e->db_fix->db_values[e->db_fix->max_step - e->db_fix->min_step];
    } else if (e->direction == PA_ALSA_DIRECTION_OUTPUT)
        e->has_dB = snd_mixer_selem_get_playback_dB_range(me, &min_dB, &max_dB) >= 0;
    else
        e->has_dB = snd_mixer_selem_get_capture_dB_range(me, &min_dB, &max_dB) >= 0;

    // 检查核心驱动返回的限制是否与 _get_*_dB_range() 和 _ask_*_vol_dB() 一致
    if (e->has_dB && !e->db_fix) {
        long min_dB_checked = 0;
        long max_dB_checked = 0;

        if (element_ask_vol_dB(me, e->direction, e->min_volume, &min_dB_checked) < 0) {
            pa_log_warn("Failed to query the dB value for %s at volume level %li", e->alsa_name, e->min_volume);
            return false;
        }

        if (element_ask_vol_dB(me, e->direction, e->max_volume, &max_dB_checked) < 0) {
            pa_log_warn("Failed to query the dB value for %s at volume level %li", e->alsa_name, e->max_volume);
            return false;
        }

        if (min_dB != min_dB_checked || max_dB != max_dB_checked) {
            pa_log_warn("Your kernel driver is broken: the reported dB range for %s (from %0.2f dB to %0.2f dB) "
                        "doesn't match the dB values at minimum and maximum volume levels: %0.2f dB at level %li, "
                        "%0.2f dB at level %li.", e->alsa_name, min_dB / 100.0, max_dB / 100.0,
                        min_dB_checked / 100.0, e->min_volume, max_dB_checked / 100.0, e->max_volume);
            return false;
        }
    }

    // 确定是否支持 dB 范围，并设置最小和最大 dB 值
    if (e->has_dB) {
        e->min_dB = ((double) min_dB) / 100.0;
        e->max_dB = ((double) max_dB) / 100.0;

        if (min_dB >= max_dB) {
            pa_assert(!e->db_fix);
            pa_log_warn("Your kernel driver is broken: it reports a volume range from %0.2f dB to %0.2f dB which makes no sense.",
                        e->min_dB, e->max_dB);
            e->has_dB = false;
        }
    }

    // 检查和设置音量限制
    if (e->volume_limit >= 0) {
        if (e->volume_limit <= e->min_volume || e->volume_limit > e->max_volume)
            pa_log_warn("Volume limit for element %s of path %s is invalid: %li isn't within the valid range "
                        "%li-%li. The volume limit is ignored.",
                        e->alsa_name, e->path->name, e->volume_limit, e->min_volume + 1, e->max_volume);
        else {
            e->max_volume = e->volume_limit;

            if (e->has_dB) {
                if (e->db_fix) {
                    e->db_fix->max_step = e->max_volume;
                    e->max_dB = ((double) e->db_fix->db_values[e->db_fix->max_step - e->db_fix->min_step]) / 100.0;
                } else if (element_ask_vol_dB(me, e->direction, e->max_volume, &max_dB) < 0) {
                    pa_log_warn("Failed to get dB value of %s: %s", e->alsa_name, pa_alsa_strerror(r));
                    e->has_dB = false;
                } else
                    e->max_dB = ((double) max_dB) / 100.0;
            }
        }
    }

    // 检查单声道情况
    if (e->direction == PA_ALSA_DIRECTION_OUTPUT)
        is_mono = snd_mixer_selem_is_playback_mono(me) > 0;
    else
        is_mono = snd_mixer_selem_is_capture_mono(me) > 0;

    // 处理单声道情况
    if (is_mono) {
        e->n_channels = 1;

        // 检查和处理单声道的覆盖映射
        if ((e->override_map & (1 << (e->n_channels-1))) && e->masks[SND_MIXER_SCHN_MONO][e->n_channels-1] == 0) {
            pa_log_warn("Override map for mono element %s is invalid, ignoring override map", e->path->name);
            e->override_map &= ~(1 << (e->n_channels-1));
        }
        if (!(e->override_map & (1 << (e->n_channels-1)))) {
            for (p = PA_CHANNEL_POSITION_FRONT_LEFT; p < PA_CHANNEL_POSITION_MAX; p++) {
                if (alsa_channel_ids[p] == SND_MIXER_SCHN_UNKNOWN)
                    continue;
                e->masks[alsa_channel_ids[p]][e->n_channels-1] = 0;
            }
            e->masks[SND_MIXER_SCHN_MONO][e->n_channels-1] = PA_CHANNEL_POSITION_MASK_ALL;
        }
        e->merged_mask = e->masks[SND_MIXER_SCHN_MONO][e->n_channels-1];
        return true;
    }

    // 处理多声道情况
    e->n_channels = 0;
    for (p = PA_CHANNEL_POSITION_FRONT_LEFT; p < PA_CHANNEL_POSITION_MAX; p++) {
        if (alsa_channel_ids[p] == SND_MIXER_SCHN_UNKNOWN)
            continue;

        if (e->direction == PA_ALSA_DIRECTION_OUTPUT)
            e->n_channels += snd_mixer_selem_has_playback_channel(me, alsa_channel_ids[p]) > 0;
        else
            e->n_channels += snd_mixer_selem_has_capture_channel(me, alsa_channel_ids[p]) > 0;
    }

    // 检查声道数是否合法
    if (e->n_channels <= 0) {
        pa_log_warn("Volume element %s with no channels?", e->alsa_name);
        return false;
    } else if (e->n_channels > POSITION_MASK_CHANNELS) {
        pa_log_warn("Volume element %s has %u channels. That's too much! I can't handle that!", e->alsa_name, e->n_channels);
        return false;
    }

retry:
    // 处理声道覆盖映射
    if (!(e->override_map & (1 << (e->n_channels-1)))) {
        for (p = PA_CHANNEL_POSITION_FRONT_LEFT; p < PA_CHANNEL_POSITION_MAX; p++) {
            bool has_channel;

            if (alsa_channel_ids[p] == SND_MIXER_SCHN_UNKNOWN)
                continue;

            if (e->direction == PA_ALSA_DIRECTION_OUTPUT)
                has_channel = snd_mixer_selem_has_playback_channel(me, alsa_channel_ids[p]) > 0;
            else
                has_channel = snd_mixer_selem_has_capture_channel(me, alsa_channel_ids[p]) > 0;

            e->masks[alsa_channel_ids[p]][e->n_channels-1] = has_channel ? PA_CHANNEL_POSITION_MASK(p) : 0;
        }
    }

    e->merged_mask = 0;
    for (p = PA_CHANNEL_POSITION_FRONT_LEFT; p < PA_CHANNEL_POSITION_MAX; p++) {
        if (alsa_channel_ids[p] == SND_MIXER_SCHN_UNKNOWN)
            continue;

        e->merged_mask |= e->masks[alsa_channel_ids[p]][e->n_channels-1];
    }

    // 处理无效的声道覆盖映射
    if (e->merged_mask == 0) {
        if (!(e->override_map & (1 << (e->n_channels-1)))) {
            pa_log_warn("Channel map for element %s is invalid", e->path->name);
            return false;
        }
        pa_log_warn("Override map for element %s has empty result, ignoring override map", e->path->name);
        e->override_map &= ~(1 << (e->n_channels-1));
        goto retry;
    }

    return true;
}
```

###### check_required

```c
#0  0x0000fffff24cffa0 in check_required (e=0xaaaaaacb62d0, me=0xaaaaaad7f6b0) at modules/alsa/alsa-mixer.c:1458
#1  0x0000fffff24d1adc in element_probe (e=0xaaaaaacb62d0, m=0xaaaaaad5f180) at modules/alsa/alsa-mixer.c:1840
#2  0x0000fffff24d63c0 in pa_alsa_path_probe (p=0xaaaaaacb3dd0, mapping=0xaaaaaab2c360, m=0xaaaaaad5f180, ignore_dB=false) at modules/alsa/alsa-mixer.c:2951
#3  0x0000fffff24db2c0 in mapping_paths_probe (m=0xaaaaaab2c360, profile=0xaaaaaab408a0, direction=PA_ALSA_DIRECTION_INPUT, used_paths=0xaaaaaac6b970)
    at modules/alsa/alsa-mixer.c:4152
#4  0x0000fffff24de19c in pa_alsa_profile_set_probe (ps=0xaaaaaab28120, dev_id=0xaaaaaab12450 "0", ss=0xaaaaaaae2fec, default_n_fragments=4, default_fragment_size_msec=25)
    at modules/alsa/alsa-mixer.c:4933
#5  0x0000fffff268bd10 in module_alsa_card_LTX_pa__init (m=0xaaaaaab0f450) at modules/alsa/module-alsa-card.c:871
#6  0x0000fffff7e67104 in pa_module_load
    (module=0xffffffffcd90, c=0xaaaaaaae2ec0, name=0xfffff26a4d98 "module-alsa-card", argument=0xaaaaaaadd8b0 "device_id=\"0\" name=\"platform-PHYT0006_00\" card_name=\"alsa_card.platform-PHYT0006_00\" namereg_fail=false tsched=yes fixed_latency_range=no ignore_dB=no deferred_volume=yes use_ucm=yes card_properties=\""...) at pulsecore/module.c:187
#7  0x0000fffff26a2d9c in verify_access (u=0xaaaaaab03700, d=0xaaaaaab071f0) at modules/module-udev-detect.c:333
#8  0x0000fffff26a32fc in card_changed (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:422
#9  0x0000fffff26a36bc in process_device (u=0xaaaaaab03700, dev=0xaaaaaaaddc50) at modules/module-udev-detect.c:464
#10 0x0000fffff26a373c in process_path (u=0xaaaaaab03700, path=0xaaaaaab18b00 "/sys/devices/platform/PHYT0006:00/sound/card0") at modules/module-udev-detect.c:481
#11 0x0000fffff26a46ac in module_udev_detect_LTX_pa__init (m=0xaaaaaab00e20) at modules/module-udev-detect.c:789
#12 0x0000fffff7e67104 in pa_module_load (module=0xffffffffcfe8, c=0xaaaaaaae2ec0, name=0xaaaaaab011b0 "module-udev-detect", argument=0x0) at pulsecore/module.c:187
#13 0x0000fffff7e50170 in pa_cli_command_load (c=0xaaaaaaae2ec0, t=0xaaaaaab011d0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:437
#14 0x0000fffff7e5774c in pa_cli_command_execute_line_stateful
    (c=0xaaaaaaae2ec0, s=0xffffffffd1c8 "load-module module-udev-detect", buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505, ifstate=0xffffffffd1c0) at pulsecore/cli-command.c:2136
#15 0x0000fffff7e57a00 in pa_cli_command_execute_file_stream (c=0xaaaaaaae2ec0, f=0xaaaaaaad58f0, buf=0xaaaaaaadaf60, fail=0xaaaaaaad5505) at pulsecore/cli-command.c:2176
#16 0x0000aaaaaaab90ac in main (argc=4, argv=0xffffffffdd98) at daemon/main.c:1112
```

```c
static int check_required(pa_alsa_element *e, snd_mixer_elem_t *me) {
    // 检查参数是否有效
    pa_assert(e);
    pa_assert(me);

    // 根据音频元素的方向和属性，检查是否存在开关
    bool has_switch;
    if (e->direction == PA_ALSA_DIRECTION_OUTPUT) {
        has_switch =
            snd_mixer_selem_has_playback_switch(me) ||
            (e->direction_try_other && snd_mixer_selem_has_capture_switch(me));
    } else {
        has_switch =
            snd_mixer_selem_has_capture_switch(me) ||
            (e->direction_try_other && snd_mixer_selem_has_playback_switch(me));
    }

    // 根据音频元素的方向和属性，检查是否存在音量控制
    bool has_volume;
    if (e->direction == PA_ALSA_DIRECTION_OUTPUT) {
        has_volume =
            snd_mixer_selem_has_playback_volume(me) ||
            (e->direction_try_other && snd_mixer_selem_has_capture_volume(me));
    } else {
        has_volume =
            snd_mixer_selem_has_capture_volume(me) ||
            (e->direction_try_other && snd_mixer_selem_has_playback_volume(me));
    }

    // 检查音频元素是否是枚举类型
    bool has_enumeration = snd_mixer_selem_is_enumerated(me);

    // 根据音频元素的各种属性，进行不同的检查
    if ((e->required == PA_ALSA_REQUIRED_SWITCH && !has_switch) ||
        (e->required == PA_ALSA_REQUIRED_VOLUME && !has_volume) ||
        (e->required == PA_ALSA_REQUIRED_ENUMERATION && !has_enumeration))
        return -1;

    if (e->required == PA_ALSA_REQUIRED_ANY && !(has_switch || has_volume || has_enumeration))
        return -1;

    if ((e->required_absent == PA_ALSA_REQUIRED_SWITCH && has_switch) ||
        (e->required_absent == PA_ALSA_REQUIRED_VOLUME && has_volume) ||
        (e->required_absent == PA_ALSA_REQUIRED_ENUMERATION && has_enumeration))
        return -1;

    if (e->required_absent == PA_ALSA_REQUIRED_ANY && (has_switch || has_volume || has_enumeration))
        return -1;

    // 根据音频元素的属性，更新 path 结构体的 req_any_present 属性
    if (e->required_any != PA_ALSA_REQUIRED_IGNORE) {
        switch (e->required_any) {
            case PA_ALSA_REQUIRED_VOLUME:
                e->path->req_any_present |= (e->volume_use != PA_ALSA_VOLUME_IGNORE);
                break;
            case PA_ALSA_REQUIRED_SWITCH:
                e->path->req_any_present |= (e->switch_use != PA_ALSA_SWITCH_IGNORE);
                break;
            case PA_ALSA_REQUIRED_ENUMERATION:
                e->path->req_any_present |= (e->enumeration_use != PA_ALSA_ENUMERATION_IGNORE);
                break;
            case PA_ALSA_REQUIRED_ANY:
                e->path->req_any_present |=
                    (e->volume_use != PA_ALSA_VOLUME_IGNORE) ||
                    (e->switch_use != PA_ALSA_SWITCH_IGNORE) ||
                    (e->enumeration_use != PA_ALSA_ENUMERATION_IGNORE);
                break;
            default:
                pa_assert_not_reached();
        }
    }

    // 如果音频元素的 enumeration_use 属性为 PA_ALSA_ENUMERATION_SELECT
    // 则检查每个选项是否满足要求，并更新 req_any_present 属性
    if (e->enumeration_use == PA_ALSA_ENUMERATION_SELECT) {
        pa_alsa_option *o;
        PA_LLIST_FOREACH(o, e->options) {
            e->path->req_any_present |= (o->required_any != PA_ALSA_REQUIRED_IGNORE) &&
                (o->alsa_idx >= 0);
            if (o->required != PA_ALSA_REQUIRED_IGNORE && o->alsa_idx < 0)
                return -1;
            if (o->required_absent != PA_ALSA_REQUIRED_IGNORE && o->alsa_idx >= 0)
                return -1;
        }
    }

    // 返回检查结果，如果满足要求返回 0，否则返回 -1
    return 0;
}
```

## 关键断点

断点保存：

```bash
save breakpoints pulseaudio.bk
```

```text
break src/modules/alsa/alsa-mixer.c:3184
disable $bpnum
break src/modules/alsa/module-alsa-card.c:849
disable $bpnum
break src/modules/alsa/module-alsa-card.c:868
disable $bpnum
break src/modules/alsa/module-alsa-card.c:871
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4520
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4577
disable $bpnum
break src/pulsecore/conf-parser.c:167
disable $bpnum
break src/pulsecore/conf-parser.c:208
disable $bpnum
break src/pulsecore/conf-parser.c:83
disable $bpnum
break src/pulsecore/conf-parser.c:157
disable $bpnum
break src/pulsecore/conf-parser.c:42
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4782
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4890
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4701
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4833
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4867
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4651
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4881
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4719
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4923
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4115
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4127
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4141
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4156
disable $bpnum
break src/modules/alsa/alsa-mixer.c:3478
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4588
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4605
disable $bpnum
break main
disable $bpnum
break /home/uos/code/pulseaudio-12.2.58/src/daemon/daemon-conf.c:710
disable $bpnum
break /home/uos/code/pulseaudio-12.2.58/src/daemon/daemon-conf.c:540
disable $bpnum
break src/pulsecore/cli-command.c:2187
disable $bpnum
break src/pulsecore/cli-command.c:2136
disable $bpnum
break src/modules/alsa/alsa-mixer.c:4738
disable $bpnum
break src/modules/alsa/alsa-mixer.c:3184
disable $bpnum
break src/modules/alsa/alsa-mixer.c:3255
disable $bpnum
break src/modules/alsa/alsa-mixer.c:3284
disable $bpnum
break src/modules/alsa/alsa-mixer.c:1872
disable $bpnum
break /home/uos/code/pulseaudio-12.2.58/src/modules/alsa/alsa-mixer.c:151
disable $bpnum
break src/modules/alsa/alsa-ucm.c:1816
disable $bpnum
break src/modules/alsa/alsa-mixer.c:3015
disable $bpnum
```

使用gdb info breakpoints查看当前断点，可以看到行号、函数名：

```bash
i b
```

```c
Num     Type           Disp Enb Address            What                                                                                                                            
1       breakpoint     keep n   0x0000fffff24d78c4 in pa_alsa_path_set_new at modules/alsa/alsa-mixer.c:3184                                                                       
2       breakpoint     keep n   0x0000fffff268bbfc in module_alsa_card_LTX_pa__init at modules/alsa/module-alsa-card.c:849                                                         
3       breakpoint     keep n   0x0000fffff268bcb0 in module_alsa_card_LTX_pa__init at modules/alsa/module-alsa-card.c:868                                                         
4       breakpoint     keep n   0x0000fffff268bcd0 in module_alsa_card_LTX_pa__init at modules/alsa/module-alsa-card.c:871                                                         
5       breakpoint     keep n   0x0000fffff24dc94c in pa_alsa_profile_set_new at modules/alsa/alsa-mixer.c:4520                                                                   
6       breakpoint     keep n   0x0000fffff24dcae4 in pa_alsa_profile_set_new at modules/alsa/alsa-mixer.c:4577                                                                   
7       breakpoint     keep n   0x0000fffff7dacf08 in pa_config_parse at pulsecore/conf-parser.c:168                                                                               
8       breakpoint     keep n   0x0000fffff7dad190 in pa_config_parse at pulsecore/conf-parser.c:208                                                                              
9       breakpoint     keep n   0x0000fffff7dac9e0 in parse_line at pulsecore/conf-parser.c:86                                                                                    
10      breakpoint     keep n   0x0000fffff7dace98 in parse_line at pulsecore/conf-parser.c:157                                                                                   
11      breakpoint     keep n   0x0000fffff7dac698 in normal_assignment at pulsecore/conf-parser.c:45                                                                              
12      breakpoint     keep n   0x0000fffff24dd714 in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4787                                                                 
13      breakpoint     keep n   0x0000fffff24ddebc in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4890                                                                 
14      breakpoint     keep n   0x0000fffff24dd250 in add_profiles_to_probe at modules/alsa/alsa-mixer.c:4705                                                                     
15      breakpoint     keep n   0x0000fffff24ddad0 in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4833                                                                 
16      breakpoint     keep n   0x0000fffff24ddd40 in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4867                                                                 
17      breakpoint     keep n   0x0000fffff24dced8 in mapping_open_pcm at modules/alsa/alsa-mixer.c:4657                                                                          
18      breakpoint     keep n   0x0000fffff24dde28 in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4881                                                                 
19      breakpoint     keep n   0x0000fffff24dd348 in mapping_query_hw_device at modules/alsa/alsa-mixer.c:4719                                                                   
20      breakpoint     keep n   0x0000fffff24de0e8 in pa_alsa_profile_set_probe at modules/alsa/alsa-mixer.c:4923                                                                 
21      breakpoint     keep y   0x0000fffff24db120 in mapping_paths_probe at modules/alsa/alsa-mixer.c:4116                                                                       
22      breakpoint     keep n   0x0000fffff24db150 in mapping_paths_probe at modules/alsa/alsa-mixer.c:4127                                                                       
23      breakpoint     keep n   0x0000fffff24db22c in mapping_paths_probe at modules/alsa/alsa-mixer.c:4141                                                                       
24      breakpoint     keep n   0x0000fffff24db2fc in mapping_paths_probe at modules/alsa/alsa-mixer.c:4156                                                                       
25      breakpoint     keep n   0x0000fffff24d8bb8 in path_set_condense at modules/alsa/alsa-mixer.c:3478                                                                         
26      breakpoint     keep n   0x0000fffff24dcb84 in pa_alsa_profile_set_new at modules/alsa/alsa-mixer.c:4588                                                                   
27      breakpoint     keep n   0x0000fffff24dcc9c in profile_finalize_probing at modules/alsa/alsa-mixer.c:4605                                                                  
28      breakpoint     keep n   0x0000aaaaaaab6fd8 in main at daemon/main.c:371                                  
29      breakpoint     keep n   0x0000aaaaaaab43ac in pa_daemon_conf_load at daemon/daemon-conf.c:710                                                                             
30      breakpoint     keep n   0x0000aaaaaaab3674 in pa_get_hw_info at daemon/daemon-conf.c:540                                                                                   
31      breakpoint     keep n   0x0000fffff7e57a88 in pa_cli_command_execute_file at pulsecore/cli-command.c:2187                                                                 
32      breakpoint     keep n   0x0000fffff7e57730 in pa_cli_command_execute_line_stateful at pulsecore/cli-command.c:2136                                                        
33      breakpoint     keep n   0x0000fffff24dd458 in pa_alsa_profile_set_cust_paths at modules/alsa/alsa-mixer.c:4747                                                            
34      breakpoint     keep n   0x0000fffff24d78c4 in pa_alsa_path_set_new at modules/alsa/alsa-mixer.c:3184                                                                      
35      breakpoint     keep n   0x0000fffff24d7d04 in pa_alsa_path_set_new at modules/alsa/alsa-mixer.c:3255                                                                      
37      breakpoint     keep n   0x0000fffff24d7f28 in pa_alsa_path_set_new at modules/alsa/alsa-mixer.c:3284                                                                      
38      breakpoint     keep n   0x0000fffff24d1cc0 in jack_probe at modules/alsa/alsa-mixer.c:1872                                                                                
39      breakpoint     keep n   0x0000fffff24ca6b8 in pa_alsa_jack_set_has_control at modules/alsa/alsa-mixer.c:151                                                               
40      breakpoint     keep n   0x0000fffff24c9c70 in pa_alsa_ucm_device_update_available at modules/alsa/alsa-ucm.c:1817                                                         
41      breakpoint     keep n   0x0000fffff24d67f0 in pa_alsa_path_probe at modules/alsa/alsa-mixer.c:3015                                                                        
42      breakpoint     keep n   0x0000aaaaaaab393c in pa_daemon_conf_load at daemon/daemon-conf.c:603                                                                             
44      breakpoint     keep n   0x0000fffff7dac9e0 in parse_line at pulsecore/conf-parser.c:86                                                                                    
45      breakpoint     keep n   0x0000aaaaaaab3674 in pa_get_hw_info at daemon/daemon-conf.c:540                                          
46      breakpoint     keep n   0x0000aaaaaaab5f3c in pa_ltdl_init at daemon/ltdl-bind-now.c:118
47      breakpoint     keep n   0x0000fffff24dbb30 in profile_set_add_auto at modules/alsa/alsa-mixer.c:4306
49      breakpoint     keep n   0x0000fffff24dd250 in add_profiles_to_probe at modules/alsa/alsa-mixer.c:4705
50      breakpoint     keep n   0x0000fffff24dced8 in mapping_open_pcm at modules/alsa/alsa-mixer.c:4657
51      breakpoint     keep n   0x0000fffff24bf814 in pa_alsa_open_by_template at modules/alsa/alsa-util.c:791
52      breakpoint     keep n   0x0000fffff24bf39c in pa_alsa_open_by_device_string at modules/alsa/alsa-util.c:686
```

断点恢复：

```bash
source pulseaudio.bk
```
