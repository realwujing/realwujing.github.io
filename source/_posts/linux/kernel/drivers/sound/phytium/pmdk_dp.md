---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

```c
dmesg [   63.395481] pmdk_dp PHYT8006:00: ASoC: CODEC DAI i2s-hifi not registered
sound/soc/soc-core.c:884:dev_err(card->dev, "ASoC: CODEC DAI %s not registered\n",
sound/soc/soc-core.c:839:soc_bind_dai_link:882 // 查找并设置 CODEC DAI 组件出错，在 ALSA SoC（Sound on Chip）架构中，DAI是“Digital Audio Interface”的缩写，表示数字音频接口
sound/soc/soc-core.c:839:soc_bind_dai_link:884
sound/soc/soc-core.c:1995:snd_soc_instantiate_card:1969
sound/soc/soc-core.c:2703:snd_soc_register_card:2738
sound/soc/soc-devres.c:63:devm_snd_soc_register_card:72
sound/soc/phytium/pmdk_dp.c:166:pmdk_sound_probe:206
sound/soc/phytium/pmdk_dp.c:233:module_platform_driver:222
```
