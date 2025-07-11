---
date: 2024/01/12 17:26:54
updated: 2024/01/12 17:26:54
---

```text
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI status: Codec=0 Pin=3 Presence_Detect=1 ELD_Valid=1
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: detected monitor  at connection type HDMI
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: available speakers: FL/FR
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: supports coding type LPCM: channels = 2, rates = 32000 44100 48000 88200 96000 176400 192000, bits = 16 20 24
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: atihdmi_pin_hbr_setup: NID=0x3, hbr-ctl=0x1
[四 11月  9 18:16:25 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_setup_stream: NID=0x2, stream=0x1, channel=0, format=0x4031
[四 11月  9 18:16:26 2023] snd_hda_codec_hdmi hdaudioC0D0: atihdmi_pin_hbr_setup: NID=0x3, hbr-ctl=0x1
[四 11月  9 18:16:26 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_setup_stream: NID=0x2, stream=0x1, channel=0, format=0x4031
[四 11月  9 18:16:26 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_cleanup_stream: NID=0x2









[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI status: Codec=0 Pin=3 Presence_Detect=1 ELD_Valid=1
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: detected monitor  at connection type HDMI
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: available speakers: FL/FR
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: supports coding type LPCM: channels = 2, rates = 32000 44100 48000 88200 96000 176400 192000, bits = 16 20 24
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: atihdmi_pin_hbr_setup: NID=0x3, hbr-ctl=0x1
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_setup_stream: NID=0x2, stream=0x1, channel=0, format=0x4031
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: atihdmi_pin_hbr_setup: NID=0x3, hbr-ctl=0x1
[四 11月  9 18:16:47 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_setup_stream: NID=0x2, stream=0x1, channel=0, format=0x4031
[四 11月  9 18:16:48 2023] snd_hda_codec_hdmi hdaudioC0D0: hda_codec_cleanup_stream: NID=0x2
```

```bash
dmesg -C && dmesg -w -T
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI status: Codec=0 Pin=3 Presence_Detect=1 ELD_Valid=1
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: detected monitor  at connection type HDMI
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: available speakers: FL/FR
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: HDMI: supports coding type LPCM: channels = 2, rates = 32000 44100 48000 88200 96000 176400 192000, bits = 16 20 24
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: hda-codec: reconfiguring
[四 11月  9 18:25:37 2023] snd_hda_codec_hdmi hdaudioC0D0: The codec is being used, can't reconfigure.
```


```text
对于存在偶尔没有声音的情况，可能是由于音频缓冲管理不当导致的。DRM/KMS 子系统负责管理 ALSA 提供的音频缓冲区，以确保音频数据的连续传输和适时播放。下面是一些可能导致偶尔没有声音的原因和相应的解决方法：

1. 缓冲区大小不合适：音频缓冲区的大小直接影响数据传输的流畅性和及时性。如果缓冲区过小，可能导致数据传输不及时，从而引发音频中断或不连贯的播放。解决方法是增加缓冲区的大小，以确保足够的数据可以存储和传输。

2. 数据传输速度不匹配：音频数据的传输速度需要与音频设备的播放速率相匹配。如果数据传输速度过快或过慢，可能导致音频中断或不连贯的播放。确保音频数据的传输速度与设备的播放速率相匹配，可以通过调整缓冲区的填充和排空速度来实现。

3. 中断处理不及时：在数据传输过程中，如果中断处理不及时，可能导致音频中断或不连贯的播放。确保中断处理程序能够及时响应音频数据的到达和处理，以确保连续的音频播放。

4. 硬件兼容性问题：有时，特定的音频设备可能与 DRM/KMS 子系统或 ALSA 驱动程序存在兼容性问题，导致偶尔没有声音。在这种情况下，可以尝试更新或更换音频设备的驱动程序，或者与设备制造商联系以获取支持。

5. 系统负载过高：如果系统负载过高，可能导致音频数据的处理和传输受到影响，从而导致偶尔没有声音。在这种情况下，优化系统配置、关闭不必要的进程或降低其他应用程序的负载可以改善音频播放的连续性。

这些是一些常见的导致偶尔没有声音的问题和解决方法。如果问题仍然存在，可能需要进一步调查和排除其他可能的因素，如硬件故障或其他软件冲突。
```

```text
在 Linux 内核中，DRM/KMS 子系统与 ALSA 驱动程序之间进行协作以管理音频缓冲区。具体涉及的函数和处理方式可能会因内核版本而异，以下是在内核版本 4.19 中常用的一些函数和处理方式的示例：

1. `drm_audio_component_register()`：该函数用于注册 DRM/KMS 子系统的音频组件。音频组件在 DRM/KMS 子系统中负责与 ALSA 驱动程序进行交互和协作。

2. `drm_audio_component_unregister()`：用于取消注册音频组件。

3. `snd_pcm_lib_ioctl()`：该函数由 ALSA 提供，用于处理 PCM 设备的 ioctl 请求。在 DRM/KMS 子系统中，可以通过该函数处理与音频缓冲区相关的 ioctl 请求，如设置缓冲区大小、填充和排空缓冲区等。

4. `drm_audio_component_pcm_open()`：用于在 DRM/KMS 子系统中打开 PCM 设备。该函数在 ALSA 驱动程序中调用，与 DRM/KMS 子系统建立音频设备的连接。

5. `drm_audio_component_pcm_close()`：用于关闭 PCM 设备连接。

6. `snd_pcm_period_elapsed()`：该函数由 ALSA 提供，用于通知 DMA 引擎当前周期的音频数据已传输完毕。在 DRM/KMS 子系统中，可以通过该函数通知 DRM/KMS 子系统进行下一周期的音频数据传输和处理。

这些函数和处理方式只是在内核版本 4.19 中的一些示例，实际的实现可能会因具体的硬件和驱动程序而有所不同。在具体的内核版本中，你可以查阅相应的内核文档和源代码来了解更详细的函数和处理方式。
```

```text
 -F, --period-time=#
              Distance between interrupts is # microseconds.  If no period time and no period size is given then a quarter of the buffer time is set.

       -B, --buffer-time=#
              Buffer duration is # microseconds If no buffer time and no buffer size is given then the maximal allowed buffer time but not more than 500ms is set.

       --period-size=#
              Distance between interrupts is # frames If no period size and no period time is given then a quarter of the buffer size is set.

       --buffer-size=#
              Buffer duration is # frames If no buffer time and no buffer size is given then the maximal allowed buffer time but not more than 500ms is set.
```

```bash
aplay --period-time=10000 --buffer-size=2048 audio.wav
aplay --period-size=256 --buffer-time=500000 audio.wav
```

- [Banana Pi BPI-W3(RK3588)之hdmirx下audio使用.](https://www.toutiao.com/article/7291483202471051817)
- [How to force HDMI audio (intel card) to be enabled despite receiving broken EDID](https://unix.stackexchange.com/questions/391326/how-to-force-hdmi-audio-intel-card-to-be-enabled-despite-receiving-broken-edid)

```bash
cat /proc/asound/card*/eld\#*
monitor_present         1
eld_valid               1
codec_pin_nid           0x3
codec_dev_id            0x0
codec_cvt_nid           0x2
monitor_name
connection_type         HDMI
eld_version             [0x2] CEA-861D or below
edid_version            [0x0] no CEA EDID Timing Extension block present
manufacture_id          0x0
product_id              0x0
port_id                 0x0
support_hdcp            0
support_ai              0
audio_sync_delay        0
speakers                [0x1] FL/FR
sad_count               1
sad0_coding_type        [0x1] LPCM
sad0_channels           2
sad0_rates              [0x1ee0] 32000 44100 48000 88200 96000 176400 192000
sad0_bits               [0xe0000] 16 20 24
```

```bash
sox /usr/share/sounds/deepin/stereo/audio-volume-change.wav audio-volume-change-tmp.wav pad 0 1
mv audio-volume-change-tmp.wav audio-volume-change.wav
```

这个命令是在音频结尾增加1秒静音，1秒建议尽量往小了调整，满足缓冲区大小就行了。

```bash
dpkg -S /usr/share/sounds/deepin/stereo/audio-volume-change.wav
deepin-sound-theme: /usr/share/sounds/deepin/stereo/audio-volume-change.wav
```
