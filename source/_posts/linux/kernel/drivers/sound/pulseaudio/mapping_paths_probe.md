---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# mapping_paths_probe

```c
static void mapping_paths_probe(pa_alsa_mapping *m, pa_alsa_profile *profile,
                                pa_alsa_direction_t direction, pa_hashmap *used_paths) {
    pa_alsa_path *p;
    void *state;
    snd_pcm_t *pcm_handle;
    pa_alsa_path_set *ps;
    snd_mixer_t *mixer_handle;

    // 根据方向和映射创建相应的路径集合
    if (direction == PA_ALSA_DIRECTION_OUTPUT) {
        if (m->output_path_set)
            return; // 如果路径集合已经存在，则直接返回，避免重复探测
        m->output_path_set = ps = pa_alsa_path_set_new(m, direction, NULL); // 创建新的输出路径集合
        pcm_handle = m->output_pcm; // 获取输出 PCM 句柄
    } else {
        if (m->input_path_set)
            return; // 如果路径集合已经存在，则直接返回，避免重复探测
        m->input_path_set = ps = pa_alsa_path_set_new(m, direction, NULL); // 创建新的输入路径集合
        pcm_handle = m->input_pcm; // 获取输入 PCM 句柄
    }

    // 如果路径集合为空，则直接返回
    if (!ps)
        return;

    pa_assert(pcm_handle);

    // 打开 PCM 句柄关联的混音器
    mixer_handle = pa_alsa_open_mixer_for_pcm(pcm_handle, NULL);
    if (!mixer_handle) {
        // 无法打开混音器，移除路径集合中的所有路径
        pa_hashmap_remove_all(ps->paths);
        return;
    }

    // 遍历路径集合中的每个路径，进行路径属性的探测和设置
    PA_HASHMAP_FOREACH(p, ps->paths, state) {
        if (p->autodetect_eld_device)
            p->eld_device = m->hw_device_index;

        // 探测和设置路径属性，如果失败，则从路径集合中移除路径
        if (pa_alsa_path_probe(p, m, mixer_handle, m->profile_set->ignore_dB) < 0)
            pa_hashmap_remove(ps->paths, p);
    }

    // 对路径集合进行整理和优化
    path_set_condense(ps, mixer_handle);
    path_set_make_path_descriptions_unique(ps);

    // 如果混音器句柄存在，则关闭混音器
    if (mixer_handle)
        snd_mixer_close(mixer_handle);

    // 将经过探测和设置属性的路径放入用于后续使用的哈希映射中
    PA_HASHMAP_FOREACH(p, ps->paths, state)
        pa_hashmap_put(used_paths, p, p);

    // 输出调试信息，显示整理后的可用混音路径
    pa_log_debug("Available mixer paths (after tidying):");
    pa_alsa_path_set_dump(ps);
}
```
