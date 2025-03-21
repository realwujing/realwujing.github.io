---
date: 2023/12/05 14:50:57
updated: 2024/09/09 13:55:05
---

# sound

- [菜鸟也能懂的 - 音视频基础知识。](https://mp.weixin.qq.com/s/HzJyf9QLZYjRsacf_veK4g)
- [音频基础知识](https://mp.weixin.qq.com/s/_EY5Y9kopd8XvtKPMltQhw)
- [深度好文 | Android高性能音频解析](https://mp.weixin.qq.com/s/yQQ5q8vvi7ltVxVQrfcl0Q)
- [<font color=Red>Linux内核音频驱动</font>](https://mp.weixin.qq.com/s/dF8m0jna-HE9LjCQ8d6H1g)

## 耳机

- [耳机插头3.5 三段与四段 的区别](https://www.jianshu.com/p/9355acdfafb2)
- [<font color=Red>3.5mm 音频接口类型说明（3极和4极）耳机接口</font>](https://blog.csdn.net/chenhuanqiangnihao/article/details/129006293)
- [三段，四段耳机与识别](https://blog.csdn.net/zz_nj/article/details/123095546)
- [<font color=Red>音频 codec—— 耳机电路</font>](https://winddoing.github.io/post/86a98cff.html)

### pop音

- [关于手机常见音频POP音产生的原因以及解决思路(一)——耳机插入与拔出](https://blog.csdn.net/weixin_43772512/article/details/126790368)

## alsa

- [<font color=Red>Linux ALSA sound notes</font>](http://www.sabi.co.uk/Notes/linuxSoundALSA.html)
- [ALSA（高级Linux声音架构）浅析](https://mp.weixin.qq.com/s/1TJQc3Ktdw_Qz576iLjB1Q)
- [Linux音频驱动-ALSA概述](https://blog.csdn.net/longwang155069/article/details/53260731)
- [ALSA配置文件(alsa.conf, asoundrc, asound.conf)及其自动加载 And HDMI Adiuo](https://blog.csdn.net/lile777/article/details/62428473)
- [kernel 4.19音频框架超详细分析(ALSA数据流程、控制流程、驱动层)](https://blog.csdn.net/l316194152/article/details/116854430)

### hda

- [<font color=Red>Pin configuration guidelines for High Definition Audio devices</font>](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/design/dn613979(v=vs.85)?redirectedfrom=MSDN)
- [<font color=Red>High Definition Audio Specification Revision 1.0a</font>](https://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf)
- [More Notes on HD-Audio Driver](https://www.kernel.org/doc/html/latest/sound/hd-audio/notes.html)

#### hda-verb

- [hda-verb参数详表](https://www.jianshu.com/p/d4e92693b17e)

#### codec

- [ALSA-hda开发笔记](https://blog.csdn.net/qq_38350702/article/details/111995259)
- [<font color=Red>Linux音频问题——codec寄存器配置</font>](https://blog.csdn.net/tombaby_come/article/details/129521118)
- [<font color=Red>HDA codec相关(2) - verbtable相关</font>](https://blog.csdn.net/qq_21186033/article/details/117667075)

##### codecgraph

- [https://github.com/cmatsuoka/codecgraph](https://github.com/cmatsuoka/codecgraph)
- [https://manpages.debian.org/unstable/codecgraph/codecgraph.1.en.html](https://manpages.debian.org/unstable/codecgraph/codecgraph.1.en.html)

    安装 codecgraph:

    ```bash
    sudo apt install codecgraph
    ```

    生成codec#0.svg:

    ```bash
    codecgraph
    ```

    codec#0.svg解读：

    ```text
    节点形状和颜色：

    放大器：三角形
    音频输入：红色椭圆形
    音频输出：蓝色椭圆形
    音频选择器：平行四边形
    音频混合器：六边形
    引脚复杂结构：矩形
    ```

## realtek

- [<font color=Red>ALC885_1-1.pdf</font>](https://github.com/realwujing/realwujing.github.io/blob/main/linux/kernel/sound/ALC885_1-1.pdf)
- [http://realtek.info/pdf/ALC885_1-1.pdf](http://realtek.info/pdf/ALC885_1-1.pdf)

### 声卡 vendor id

查看 Linux 系统中声卡的 vendor ID：

```bash
cat /proc/asound/card*/codec#* | grep "Vendor Id" -B3
```

### gain、volume

- [两个单词 | 彻底搞懂增益Gain和音量Volume的区别？！](https://www.bilibili.com/read/cv3616606)
- [Volume and Loudness](https://webaudioapi.com/book/Web_Audio_API_Boris_Smus_html/ch03.html)

#### 增益、音量都是调整波形的振幅吗？

是的，增益和音量调整都涉及到调整声音波形的振幅，但它们在不同的背景下使用，可能有些微的区别：

1. **增益（Gain）：** 增益通常用于放大信号的振幅，使声音变得更响亮或更明显。在音频处理中，增益常用于放大弱音频信号，以提升其音量，同时可能也会增加噪音的幅度。增益可以应用于整个信号，或者在频域上进行分段增益调整。

2. **音量（Volume）：** 音量是指听到的声音的相对强度级别。在实际应用中，音量调整通常涉及到调整整个声音信号的振幅，以控制声音的响度。音量调整可以根据用户的偏好来控制声音的大小，通常通过音频播放设备的硬件或软件控制进行调整。

总的来说，增益和音量都可以影响声音波形的振幅，但它们的使用背景和调整方式可能会有一些不同。增益主要用于信号处理和音频增强，而音量主要用于用户控制和调整听到的声音的强度。

### 音调（Pitch）和分贝（dB）的区别？

音调（Pitch）和分贝（dB）是声音属性的两个不同方面，它们分别描述了声音的频率和声音的强度。

音调（Pitch）： 音调是描述声音频率的属性，通常以赫兹（Hz）为单位。不同频率的声音听起来会有不同的音高，音调较高的声音听起来更尖锐，音调较低的声音听起来更低沉。音调与声音的频率直接相关，较高的频率对应较高的音调，较低的频率对应较低的音调。

分贝（Decibel，dB）： 分贝是一种用于描述声音强度或信号强度的单位。声音的强度是声音能量的度量，通常以分贝为单位来表示。分贝是一个对数单位，用于表示相对于参考值的比率。在声学中，通常使用分贝来描述声音的相对强度。例如，分贝可以用来衡量声音的音量大小。

总之，音调和分贝是声音属性的两个不同方面。音调描述声音的频率特性，而分贝描述声音的强度特性。它们在声学中具有不同的作用和意义。

#### 音调（Pitch）和分贝（dB）在波形上的区别？

在声音的波形上，音调和分贝可以通过不同的特征来展现出来：

1. **音调（Pitch）：** 在波形上，音调主要体现在周期性振动的频率上。高音调的声音波形会有更快的周期性振动，而低音调的声音波形则会有较慢的周期性振动。例如，高音调的声音类似于快速连续的波峰和波谷，而低音调的声音则类似于较为缓慢的波动。

2. **分贝（Decibel，dB）：** 在波形上，分贝主要体现声音的振幅或幅度。较高分贝的声音波形会有更大的振幅，而较低分贝的声音波形则会有较小的振幅。分贝的增加意味着声音的强度增加，波形的振幅变大。

需要注意的是，音调和分贝是两个不同的声音属性，它们在波形上展现的特征不同，因为它们分别描述了声音的频率和强度。在波形上观察时，音调和分贝可以提供关于声音特性的不同视觉表现。

#### 上面的频率是采样率吗？

抱歉，我之前的回答中可能有些混淆。上面提到的“频率”一词并不特指采样率，而是指声音信号的频率，即声音的音调。

采样率是指在数字音频处理中，将连续的声音信号转换为离散的数字信号时，每秒钟采集的样本数。采样率决定了数字音频的精度和最高频率。声音信号的频率是指声音的音调，比如高音和低音。调整采样率会影响声音的音调，但通常不是变换男声为女声的主要方式。

变换男声为女声或进行其他声音效果处理时，通常涉及到声音信号的处理，如频谱分析、谐波调制、声音合成等，以调整声音的音调、谐波比例和频谱特性。声音信号的处理可以通过数字信号处理（DSP）算法来实现，从而改变声音的特性，实现从男声到女声的变换效果。

##### 频率是波形的横轴吗

是的，频率通常表示波形在横轴上的一个维度。在声音信号中，横轴代表时间，纵轴代表信号的幅度。频率表示了波形的周期性，即波形振动的快慢，也对应于声音的音调。不同频率的波形对应不同的音调，频率越高，音调越高。在频域分析中，波形在频率轴上的变化情况可以反映出声音的频谱特性。

## pulseaudio

- [https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/)
- [https://gavv.net/articles/pulseaudio-under-the-hood/](https://gavv.net/articles/pulseaudio-under-the-hood/)
- [<font color=Red>https://www.freedesktop.org/wiki/Software/PulseAudio/Backends/ALSA/Profiles/</font>](https://www.freedesktop.org/wiki/Software/PulseAudio/Backends/ALSA/Profiles/)
- [linux音频声卡 pulseaudio服务](https://blog.csdn.net/qq_42138566/article/details/108626378)
- [PulseAudio](https://magodo.github.io/PulseAudio/)
- [https://wiki.archlinuxcn.org/wiki/PulseAudio](https://wiki.archlinuxcn.org/wiki/PulseAudio)
- [Linux使用PulseAudio设置音频card的默认profile](https://blog.csdn.net/qq_35156410/article/details/107401642)
- [Linux音频系统研究(ALSA Udev PulseAudio)](https://blog.mxslly.com/archives/174.html)
- [Tell PulseAudio to ignore a USB device using udev](https://jamielinux.com/blog/tell-pulseaudio-to-ignore-a-usb-device-using-udev/)
- [https://wiki.archlinux.org/title/PulseAudio/Examples](https://wiki.archlinux.org/title/PulseAudio/Examples)
- [https://wiki.archlinux.org/title/PulseAudio/Troubleshooting](https://wiki.archlinux.org/title/PulseAudio/Troubleshooting)
- [https://wiki.debian.org/PulseAudio](https://wiki.debian.org/PulseAudio)
- [Pulseaudio调试技巧](https://www.codenong.com/cs105336370/)
- [How to debug PulseAudio problems](https://fedoraproject.org/wiki/How_to_debug_PulseAudio_problems)
- [pulseaudio调试信息输出控制机制](https://blog.csdn.net/cgipro/article/details/6089349)
- `set args --log-level=4 --log-meta=1 --log-time=1  --log-backtrace=FRAMES`

## audacity

- [https://www.audacityteam.org/](https://www.audacityteam.org/)
- [https://support.audacityteam.org/](https://support.audacityteam.org/)

## qemu

- [QEMU 中音频模拟如何工作](https://blog.csdn.net/tq08g2z/article/details/78908757)
- [qemu声卡模拟原理-声音播放(pa+ac97)](https://blog.csdn.net/qq_16054639/article/details/116745416)

## 降噪算法

- [<font color=Red>音频信号处理算法介绍</font>](https://zhuanlan.zhihu.com/p/430811547)
- [语音降噪/语音增强的几种算法](https://blog.csdn.net/kaixinshier/article/details/72477679)
- [有没有开源的音频AI降噪库？](https://www.zhihu.com/question/602700859)
- [一种简单高效的音频降噪算法示例(附完整C代码)](https://www.cnblogs.com/cpuimage/p/10800768.html)
- [<font color=Red>https://github.com/realwujing/rnnoise</font>](https://github.com/realwujing/rnnoise)
- [<font color=Red>语音降噪是如何扼住噪声的咽喉的</font>](https://www.toutiao.com/article/7189091215299019320)
- [<font color=Red>https://github.com/werman/noise-suppression-for-voice</font>](https://github.com/werman/noise-suppression-for-voice)

## 其他

- [黑苹果定制声卡驱动（ALC892为例)](https://www.jianshu.com/p/29a74f0664f1)
- [飞腾 X100](https://www.phytium.com.cn/homepage/download/)
- [FFmpeg解码播放视频原理](https://mp.weixin.qq.com/s/W-L6JkY1wSuzsbMMNxko3Q)
