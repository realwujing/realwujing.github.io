---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# pa_alsa_profile_set_probe

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
