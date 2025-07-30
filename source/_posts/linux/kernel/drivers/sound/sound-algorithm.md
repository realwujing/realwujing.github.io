---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# sound-algorithm

源码基于`/home/wujing/code/linux-6.1.27/sound`

```bash
grep algorithm linux/sound -inr --color --include=*.{h,c}

./soc/intel/atom/sst-atom-controls.c:292: * @ids: list of algorithms
./soc/intel/atom/sst-atom-controls.c:390: * sst_send_gain_cmd - send the gain algorithm IPC to the FW
```

```c
./soc/intel/atom/sst-atom-controls.c:390: * sst_send_gain_cmd - send the gain algorithm IPC to the FW

/**
 * sst_send_gain_cmd - send the gain algorithm IPC to the FW
 * @drv: sst_data
 * @gv:the stored value of gain (also contains rampduration)
 * @task_id: task index
 * @loc_id: location/position index
 * @module_id: module index
 * @mute: flag that indicates whether this was called from the
 *  digital_mute callback or directly. If called from the
 *  digital_mute callback, module will be muted/unmuted based on this
 *  flag. The flag is always 0 if called directly.
 *
 * Called with sst_data.lock held
 *
 * The user-set gain value is sent only if the user-controllable 'mute' control
 * is OFF (indicated by gv->mute). Otherwise, the mute value (MIN value) is
 * sent.
 */
static int sst_send_gain_cmd(struct sst_data *drv, struct sst_gain_value *gv,
			      u16 task_id, u16 loc_id, u16 module_id, int mute)
```

```c
./soc/intel/atom/sst-atom-controls.c:292: * @ids: list of algorithms

/**
 * sst_find_and_send_pipe_algo - send all the algo parameters for a pipe    // 4.19内核代码中没有
 * @drv: sst_data
 * @pipe: string identifier
 * @ids: list of algorithms
 * The algos which are in each pipeline are sent to the firmware one by one
 *
 * Called with lock held
 */
static int sst_find_and_send_pipe_algo(struct sst_data *drv,
					const char *pipe, struct sst_ids *ids)
```

```c
static const struct snd_soc_dapm_widget sst_dapm_widgets[] = {
	SST_AIF_IN("modem_in", sst_set_be_modules),
	SST_AIF_IN("codec_in0", sst_set_be_modules),
	SST_AIF_IN("codec_in1", sst_set_be_modules),
	SST_AIF_OUT("modem_out", sst_set_be_modules),
	SST_AIF_OUT("codec_out0", sst_set_be_modules),
	SST_AIF_OUT("codec_out1", sst_set_be_modules),

	/* Media Paths */
	/* MediaX IN paths are set via ALLOC, so no SET_MEDIA_PATH command */
	SST_PATH_INPUT("media0_in", SST_TASK_MMX, SST_SWM_IN_MEDIA0, sst_generic_modules_event),
	SST_PATH_INPUT("media1_in", SST_TASK_MMX, SST_SWM_IN_MEDIA1, NULL),
	SST_PATH_INPUT("media2_in", SST_TASK_MMX, SST_SWM_IN_MEDIA2, sst_set_media_path),
	SST_PATH_INPUT("media3_in", SST_TASK_MMX, SST_SWM_IN_MEDIA3, NULL),
	SST_PATH_OUTPUT("media0_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA0, sst_set_media_path),
	SST_PATH_OUTPUT("media1_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA1, sst_set_media_path),

	/* SBA PCM Paths */
	SST_PATH_INPUT("pcm0_in", SST_TASK_SBA, SST_SWM_IN_PCM0, sst_set_media_path),
	SST_PATH_INPUT("pcm1_in", SST_TASK_SBA, SST_SWM_IN_PCM1, sst_set_media_path),
	SST_PATH_OUTPUT("pcm0_out", SST_TASK_SBA, SST_SWM_OUT_PCM0, sst_set_media_path),
	SST_PATH_OUTPUT("pcm1_out", SST_TASK_SBA, SST_SWM_OUT_PCM1, sst_set_media_path),
	SST_PATH_OUTPUT("pcm2_out", SST_TASK_SBA, SST_SWM_OUT_PCM2, sst_set_media_path),

	/* SBA Loops */
	SST_PATH_INPUT("sprot_loop_in", SST_TASK_SBA, SST_SWM_IN_SPROT_LOOP, NULL),
	SST_PATH_INPUT("media_loop1_in", SST_TASK_SBA, SST_SWM_IN_MEDIA_LOOP1, NULL),
	SST_PATH_INPUT("media_loop2_in", SST_TASK_SBA, SST_SWM_IN_MEDIA_LOOP2, NULL),
	SST_PATH_MEDIA_LOOP_OUTPUT("sprot_loop_out", SST_TASK_SBA, SST_SWM_OUT_SPROT_LOOP, SST_FMT_STEREO, sst_set_media_loop),
	SST_PATH_MEDIA_LOOP_OUTPUT("media_loop1_out", SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP1, SST_FMT_STEREO, sst_set_media_loop),
	SST_PATH_MEDIA_LOOP_OUTPUT("media_loop2_out", SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP2, SST_FMT_STEREO, sst_set_media_loop),

	/* Media Mixers */
	SST_SWM_MIXER("media0_out mix 0", SND_SOC_NOPM, SST_TASK_MMX, SST_SWM_OUT_MEDIA0,
		      sst_mix_media0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media1_out mix 0", SND_SOC_NOPM, SST_TASK_MMX, SST_SWM_OUT_MEDIA1,
		      sst_mix_media1_controls, sst_swm_mixer_event),

	/* SBA PCM mixers */
	SST_SWM_MIXER("pcm0_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_PCM0,
		      sst_mix_pcm0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("pcm1_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_PCM1,
		      sst_mix_pcm1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("pcm2_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_PCM2,
		      sst_mix_pcm2_controls, sst_swm_mixer_event),

	/* SBA Loop mixers */
	SST_SWM_MIXER("sprot_loop_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_SPROT_LOOP,
		      sst_mix_sprot_l0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media_loop1_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP1,
		      sst_mix_media_l1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media_loop2_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP2,
		      sst_mix_media_l2_controls, sst_swm_mixer_event),

	/* SBA Backend mixers */
	SST_SWM_MIXER("codec_out0 mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_CODEC0,
		      sst_mix_codec0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("codec_out1 mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_CODEC1,
		      sst_mix_codec1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("modem_out mix 0", SND_SOC_NOPM, SST_TASK_SBA, SST_SWM_OUT_MODEM,
		      sst_mix_modem_controls, sst_swm_mixer_event),

};
```

```c
static int sst_set_media_path(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *k, int event)
{
	int ret = 0;
	struct sst_cmd_set_media_path cmd;
	struct snd_soc_component *c = snd_soc_dapm_to_component(w->dapm);
	struct sst_data *drv = snd_soc_component_get_drvdata(c);
	struct sst_ids *ids = w->priv;

	dev_dbg(c->dev, "widget=%s\n", w->name);
	dev_dbg(c->dev, "task=%u, location=%#x\n",
				ids->task_id, ids->location_id);

	if (SND_SOC_DAPM_EVENT_ON(event))
		cmd.switch_state = SST_PATH_ON;
	else
		cmd.switch_state = SST_PATH_OFF;

	SST_FILL_DESTINATION(2, cmd.header.dst,
			     ids->location_id, SST_DEFAULT_MODULE_ID);

	/* MMX_SET_MEDIA_PATH == SBA_SET_MEDIA_PATH */
	cmd.header.command_id = MMX_SET_MEDIA_PATH;
	cmd.header.length = sizeof(struct sst_cmd_set_media_path)
				- sizeof(struct sst_dsp_header);

	ret = sst_fill_and_send_cmd(drv, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
			      ids->task_id, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);
	if (ret)
		return ret;

	if (SND_SOC_DAPM_EVENT_ON(event))
		ret = sst_send_pipe_module_params(w, k);
	return ret;
}
```

```c
grep sst_set_media_path . -inr --color --include=*.{h,c}
./soc/intel/atom/sst-atom-controls.c:997:static int sst_set_media_path(struct snd_soc_dapm_widget *w,
./soc/intel/atom/sst-atom-controls.c:1084:      SST_PATH_INPUT("media2_in", SST_TASK_MMX, SST_SWM_IN_MEDIA2, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1086:      SST_PATH_OUTPUT("media0_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA0, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1087:      SST_PATH_OUTPUT("media1_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA1, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1090:      SST_PATH_INPUT("pcm0_in", SST_TASK_SBA, SST_SWM_IN_PCM0, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1091:      SST_PATH_INPUT("pcm1_in", SST_TASK_SBA, SST_SWM_IN_PCM1, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1092:      SST_PATH_OUTPUT("pcm0_out", SST_TASK_SBA, SST_SWM_OUT_PCM0, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1093:      SST_PATH_OUTPUT("pcm1_out", SST_TASK_SBA, SST_SWM_OUT_PCM1, sst_set_media_path),
./soc/intel/atom/sst-atom-controls.c:1094:      SST_PATH_OUTPUT("pcm2_out", SST_TASK_SBA, SST_SWM_OUT_PCM2, sst_set_media_path),
```

```c
struct sst_ids {
    u16 location_id;                      // 音频处理的位置标识
    u16 module_id;                        // 音频处理模块的标识
    u8  task_id;                          // 音频处理任务的标识
    u8  format;                           // 音频数据的格式
    u8  reg;                              // 与音频处理相关的寄存器
    const char *parent_wname;             // 父级的控制名称
    struct snd_soc_dapm_widget *parent_w; // 父级控件
    struct list_head algo_list;           // 与音频算法相关的链表
    struct list_head gain_list;           // 与音频增益相关的链表
    const struct sst_pcm_format *pcm_fmt; // 音频PCM格式的配置信息
};
```

```c
grep algo_list . -inr --color --include=*.{h,c}

/**
 * sst_fill_module_list - 填充模块（module）列表
 * @kctl: kcontrol 指针
 * @w: dapm widget 指针
 * @type: 控件类型
 *
 * 填充 kcontrol 私有数据中的 widget 指针，并在 widget 私有数据中填充 kcontrol 指针。
 *
 * widget 指针用于在控件处于启动状态时在 .put() 处理程序中发送算法/增益。
 *
 * kcontrol 指针用于在控件启动/关闭事件处理程序中发送算法/增益。
 * 每个 widget（pipe）在 algo_list 中存储了多个算法。
 *
 * @return: 成功返回 0，失败返回负数错误码
 */
static int sst_fill_module_list(struct snd_kcontrol *kctl,
	 struct snd_soc_dapm_widget *w, int type)
{
	struct sst_module *module; // 模块结构体指针
	struct snd_soc_component *c = snd_soc_dapm_to_component(w->dapm); // 获取与 dapm widget 相关联的音频组件
	struct sst_ids *ids = w->priv; // 获取 widget 的私有数据
	int ret = 0;

	// 分配并初始化一个模块结构体
	module = devm_kzalloc(c->dev, sizeof(*module), GFP_KERNEL);
	if (!module)
		return -ENOMEM;

	if (type == SST_MODULE_GAIN) { // 如果控件类型是增益
		struct sst_gain_mixer_control *mc = (void *)kctl->private_value;

		mc->w = w; // 将 widget 指针存储在增益控件的私有数据中
		module->kctl = kctl; // 将 kcontrol 指针存储在模块结构体中
		list_add_tail(&module->node, &ids->gain_list); // 将模块添加到增益列表中
	} else if (type == SST_MODULE_ALGO) { // 如果控件类型是算法
		struct sst_algo_control *bc = (void *)kctl->private_value;

		bc->w = w; // 将 widget 指针存储在算法控件的私有数据中
		module->kctl = kctl; // 将 kcontrol 指针存储在模块结构体中
		list_add_tail(&module->node, &ids->algo_list); // 将模块添加到算法列表中
	} else {
		dev_err(c->dev, "invoked for unknown type %d module %s",
				type, kctl->id.name);
		ret = -EINVAL;
	}

	return ret; // 返回操作结果
}
```

```c
grep gain_list . -inr --color --include=*.{h,c}

/**
 * sst_fill_module_list - 填充模块（module）列表
 * @kctl: kcontrol 指针
 * @w: dapm widget 指针
 * @type: 控件类型
 *
 * 填充 kcontrol 私有数据中的 widget 指针，并在 widget 私有数据中填充 kcontrol 指针。
 *
 * 当 widget 处于启动状态时，widget 指针用于在 .put() 处理程序中发送算法/增益。
 *
 * 当控件的启动/关闭事件发生时，kcontrol 指针用于在事件处理程序中发送算法/增益。
 * 每个控件（pipe）都有多个算法存储在 algo_list 中。
 *
 * @return: 成功返回 0，失败返回负数错误码
 */
static int sst_fill_module_list(struct snd_kcontrol *kctl,
	 struct snd_soc_dapm_widget *w, int type)
{
	struct sst_module *module; // 模块结构体指针
	struct snd_soc_component *c = snd_soc_dapm_to_component(w->dapm); // 获取与 dapm widget 相关联的音频组件
	struct sst_ids *ids = w->priv; // 获取 widget 的私有数据
	int ret = 0;

	// 分配并初始化一个模块结构体
	module = devm_kzalloc(c->dev, sizeof(*module), GFP_KERNEL);
	if (!module)
		return -ENOMEM;

	if (type == SST_MODULE_GAIN) { // 如果控件类型是增益
		struct sst_gain_mixer_control *mc = (void *)kctl->private_value;

		mc->w = w; // 将 widget 指针存储在增益控件的私有数据中
		module->kctl = kctl; // 将 kcontrol 指针存储在模块结构体中
		list_add_tail(&module->node, &ids->gain_list); // 将模块添加到增益列表中
	} else if (type == SST_MODULE_ALGO) { // 如果控件类型是算法
		struct sst_algo_control *bc = (void *)kctl->private_value;

		bc->w = w; // 将 widget 指针存储在算法控件的私有数据中
		module->kctl = kctl; // 将 kcontrol 指针存储在模块结构体中
		list_add_tail(&module->node, &ids->algo_list); // 将模块添加到算法列表中
	} else {
		dev_err(c->dev, "invoked for unknown type %d module %s",
				type, kctl->id.name);
		ret = -EINVAL;
	}

	return ret; // 返回操作结果
}
```

## More

- [Dynamic Audio Power Management for Portable Devices](https://www.kernel.org/doc/html/latest/sound/soc/dapm.html)
