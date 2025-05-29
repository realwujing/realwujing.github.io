---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# pa_alsa_profile_set_new

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
