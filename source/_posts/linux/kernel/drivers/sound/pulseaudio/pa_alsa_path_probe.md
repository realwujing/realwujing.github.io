---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# pa_alsa_path_probe

```c
int pa_alsa_path_probe(pa_alsa_path *p, pa_alsa_mapping *mapping, snd_mixer_t *m, bool ignore_dB) {
    pa_alsa_element *e;
    pa_alsa_jack *j;
    double min_dB[PA_CHANNEL_POSITION_MAX], max_dB[PA_CHANNEL_POSITION_MAX];
    pa_channel_position_t t;
    pa_channel_position_mask_t path_volume_channels = 0;

    pa_assert(p);
    pa_assert(m);

    // 如果路径已经被探测过，返回其是否受支持的状态
    if (p->probed)
        return p->supported ? 0 : -1;
    p->probed = true;

    // 初始化 min_dB 和 max_dB 数组
    pa_zero(min_dB);
    pa_zero(max_dB);

    // 输出调试信息，表示正在探测此路径
    pa_log_debug("Probing path '%s'", p->name);

    // 遍历路径中的所有插孔，进行探测
    PA_LLIST_FOREACH(j, p->jacks) {
        // 调用 jack_probe 函数对插孔进行探测
        if (jack_probe(j, mapping, m) < 0) {
            // 插孔探测失败，标记路径不受支持，并返回错误
            p->supported = false;
            pa_log_debug("Probe of jack '%s' failed.", j->alsa_name);
            return -1;
        }
        // 输出调试信息，表示插孔探测成功
        pa_log_debug("Probe of jack '%s' succeeded (%s)", j->alsa_name, j->has_control ? "found!" : "not found");
    }

    // 遍历路径中的所有元素，进行探测
    PA_LLIST_FOREACH(e, p->elements) {
        // 调用 element_probe 函数对元素进行探测
        if (element_probe(e, m) < 0) {
            // 元素探测失败，标记路径不受支持，并返回错误
            p->supported = false;
            pa_log_debug("Probe of element '%s' failed.", e->alsa_name);
            return -1;
        }
        // 输出调试信息，表示元素探测成功，并显示其音量、开关和枚举使用情况
        pa_log_debug("Probe of element '%s' succeeded (volume=%d, switch=%d, enumeration=%d).", e->alsa_name, e->volume_use, e->switch_use, e->enumeration_use);

        // 如果忽略 dB 信息，将 has_dB 设置为 false
        if (ignore_dB)
            e->has_dB = false;

        if (e->volume_use == PA_ALSA_VOLUME_MERGE) {
            // 处理合并音量的情况

            // 如果路径中尚无音量信息，初始化 min_volume 和 max_volume
            if (!p->has_volume) {
                p->min_volume = e->min_volume;
                p->max_volume = e->max_volume;
            }

            if (e->has_dB) {
                // 如果元素有 dB 信息，处理 dB 音量

                // 如果路径中尚无音量信息，初始化 min_dB 和 max_dB，并记录音量通道位置
                if (!p->has_volume) {
                    for (t = 0; t < PA_CHANNEL_POSITION_MAX; t++)
                        if (PA_CHANNEL_POSITION_MASK(t) & e->merged_mask) {
                            min_dB[t] = e->min_dB;
                            max_dB[t] = e->max_dB;
                            path_volume_channels |= PA_CHANNEL_POSITION_MASK(t);
                        }

                    p->has_dB = true;
                } else {
                    // 如果路径中已经存在音量信息，更新 min_dB 和 max_dB，以及音量通道位置
                    if (p->has_dB) {
                        for (t = 0; t < PA_CHANNEL_POSITION_MAX; t++)
                            if (PA_CHANNEL_POSITION_MASK(t) & e->merged_mask) {
                                min_dB[t] += e->min_dB;
                                max_dB[t] += e->max_dB;
                                path_volume_channels |= PA_CHANNEL_POSITION_MASK(t);
                            }
                    } else {
                        /* 哎呀，我们前面有另一个元素无法使用 dB 音量，所以我们需要 "中和" 这个滑块 */
                        e->volume_use = PA_ALSA_VOLUME_ZERO;
                        pa_log_info("Zeroing volume of '%s' on path '%s'", e->alsa_name, p->name);
                    }
                }
            } else if (p->has_volume) {
                /* 我们无法使用这个音量，所以我们将其忽略 */
                e->volume_use = PA_ALSA_VOLUME_IGNORE;
                pa_log_info("Ignoring volume of '%s' on path '%s' (missing dB info)", e->alsa_name, p->name);
            }
            p->has_volume = true;
        }

        if (e->switch_use == PA_ALSA_SWITCH_MUTE)
            p->has_mute = true;
    }

    // 如果路径需要任何元素，但没有预置的元素，则标记路径不受支持
    if (p->has_req_any && !p->req_any_present) {
        p->supported = false;
        pa_log_debug("Skipping path '%s', none of required-any elements preset.", p->name);
        return -1;
    }

    // 丢弃不受支持的元素
    path_drop_unsupported(p);
    // 确保路径选项唯一
    path_make_options_unique(p);
    // 为路径创建设置项

    // 标记路径受支持
    p->supported = true;

    // 初始化路径的最小和最大 dB 值
    p->min_dB = INFINITY;
    p->max_dB = -INFINITY;

    // 计算路径的最小和最大 dB 值
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
