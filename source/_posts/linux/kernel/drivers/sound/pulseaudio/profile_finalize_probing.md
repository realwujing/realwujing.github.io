---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# profile_finalize_probing

```c
static void profile_finalize_probing(pa_alsa_profile *to_be_finalized, pa_alsa_profile *next) {
    pa_alsa_mapping *m;
    uint32_t idx;

    // 如果没有需要完成探测的配置文件，则直接返回
    if (!to_be_finalized)
        return;

    // 对于输出映射，执行以下操作
    if (to_be_finalized->output_mappings)
        PA_IDXSET_FOREACH(m, to_be_finalized->output_mappings, idx) {

            // 如果没有输出 PCM 流，则跳过
            if (!m->output_pcm)
                continue;

            // 如果当前配置文件是受支持的，则增加映射的支持计数
            if (to_be_finalized->supported)
                m->supported++;

            // 如果下一个配置文件也包含此映射，则不关闭 PCM 句柄，因为会立即重新打开
            if (next && next->output_mappings && pa_idxset_get_by_data(next->output_mappings, m, NULL))
                continue;

            // 关闭输出 PCM 句柄
            snd_pcm_close(m->output_pcm);
            m->output_pcm = NULL;
        }

    // 对于输入映射，执行以下操作
    if (to_be_finalized->input_mappings)
        PA_IDXSET_FOREACH(m, to_be_finalized->input_mappings, idx) {

            // 如果没有输入 PCM 流，则跳过
            if (!m->input_pcm)
                continue;

            // 如果当前配置文件是受支持的，则增加映射的支持计数
            if (to_be_finalized->supported)
                m->supported++;

            // 如果下一个配置文件也包含此映射，则不关闭 PCM 句柄，因为会立即重新打开
            if (next && next->input_mappings && pa_idxset_get_by_data(next->input_mappings, m, NULL))
                continue;

            // 关闭输入 PCM 句柄
            snd_pcm_close(m->input_pcm);
            m->input_pcm = NULL;
        }
}
```
