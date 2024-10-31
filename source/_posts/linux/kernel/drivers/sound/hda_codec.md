---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

mv /usr/bin/pulseaudio /usr/bin/pulseaudio_bak
mv /sbin/alsactl /sbin/alsactl_bak

ps aux | grep -i alsactl
ps aux | grep -i pulseaudio

killall alsactl pulseaudio

cd /sys/class/sound/hwC0D0

echo 1 > reconfig

snd_hda_gen_parse_auto_config+0x1 [kernel]
snd_hda_parse_generic_codec+0xa3 [kernel]
hda_codec_driver_probe+0x74 [kernel]
really_probe+0x24b [kernel]
driver_probe_device+0xb3 [kernel]
bus_for_each_drv+0x76 [kernel]
__device_attach+0xe5 [kernel]
snd_hda_codec_configure+0xe6 [kernel]
azx_codec_configure+0x2f [kernel]
azx_probe_continue+0x9d0 [kernel]
process_one_work+0x1a7 [kernel]
worker_thread+0x30 [kernel]
kthread+0x112 [kernel]
ret_from_fork+0x35 [kernel]


snd_pcm_new+0x1 [kernel]
snd_hda_attach_pcm_stream+0x8e [kernel]
snd_hda_codec_build_pcms+0x102 [kernel]
hda_codec_driver_probe+0x82 [kernel]
really_probe+0x24b [kernel]
driver_probe_device+0xb3 [kernel]
bus_for_each_drv+0x76 [kernel]
__device_attach+0xe5 [kernel]
snd_hda_codec_configure+0xe6 [kernel]
azx_codec_configure+0x2f [kernel]
azx_probe_continue+0x9d0 [kernel]
process_one_work+0x1a7 [kernel]
worker_thread+0x30 [kernel]
kthread+0x112 [kernel]
ret_from_fork+0x35 [kernel]




./sound/pci/hda/hda_sysfs.c:436:int snd_hda_get_bool_hint(struct hda_codec *codec, const char *key)
./sound/pci/hda/hda_sysfs.c:420:const char *snd_hda_get_hint(struct hda_codec *codec, const char *key)
./sound/pci/hda/hda_sysfs.c:281:static struct hda_hint *get_hint(struct hda_codec *codec, const char *key)

./sound/pci/hda/hda_auto_parser.c:176:int snd_hda_parse_pin_defcfg(struct hda_codec *codec, struct auto_pin_cfg *cfg, const hda_nid_t *ignore_nids, unsigned int cond_flags)
./sound/pci/hda/hda_codec.c:unsigned int snd_hda_codec_get_pincfg(struct hda_codec *codec, hda_nid_t nid)

sound/pci/hda/hda_codec.c:560:init_pins

./sound/pci/hda/hda_codec.c:445:static int read_pin_defaults(struct hda_codec *codec)

混音器 pcm
转换器 codec
端 card
