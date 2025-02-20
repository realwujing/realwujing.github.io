---
date: 2024/01/12 17:26:54
updated: 2024/01/18 16:08:47
---

# realtek ALC257 renmaed to ALC269

realtek ALC257 renmaed to ALC269

## patch

具体参见bd7c9e3.diff

## 修复链路

```c
module_hda_codec_driver(realtek_driver);    // vim sound/pci/hda/patch_realtek.c+9161
    .id = snd_hda_id_realtek,   // vim sound/pci/hda/patch_realtek.c+9158
        HDA_CODEC_ENTRY(0x10ec0257, "ALC257", patch_alc269),    // vim sound/pci/hda/patch_realtek.c+9074
            snd_hda_pick_fixup(codec, alc269_fixup_models, alc269_fixup_tbl, alc269_fixups);    // vim sound/pci/hda/patch_realtek.c+8013
                SND_PCI_QUIRK(0x10ec, 0x12f0, "BXC NF271B FT board", ALC257_FIXUP_FT_RENAME),   // vim sound/pci/hda/patch_realtek.c+7151
                    [ALC257_FIXUP_FT_RENAME] = {.type = HDA_FIXUP_FUNC,.v.func = alc_fixup_ft_alc257_rename,},   // vim sound/pci/hda/patch_realtek.c +6960
                        static void alc_fixup_ft_alc257_rename(struct hda_codec *codec, const struct hda_fixup *fix, int action)    // vim sound/pci/hda/patch_realtek.c +5721
```

在提供的代码片段中：

HDA_CODEC_ENTRY(0x10ec0257, "ALC257", patch_alc269)：

0x10ec0257 是 HDA（High Definition Audio）音频编解码器的ID。它是唯一标识 ALC257 音频编解码器的数字。

"ALC257" 是音频编解码器的人类可读名称，通常反映了制造商和型号。

patch_alc269 是一个用于修复或配置 ALC257 音频编解码器的函数或补丁。

SND_PCI_QUIRK(0x10ec, 0x12f0, "BXC NF271B FT board", ALC257_FIXUP_FT_RENAME)：

0x10ec 是 Realtek Semiconductor Corp. 的 PCI 厂商ID，用于唯一标识 Realtek 的硬件设备。

0x12f0 是一个特定 PCI Subsystem ID（子系统ID），与 "BXC NF271B FT board" 相关联。这是为了应用于特定硬件设备的修复或怪癖。

"BXC NF271B FT board" 是与子系统ID相关的人类可读描述。

ALC257_FIXUP_FT_RENAME 是一个与 "BXC NF271B FT board" 相关的修复或解决方案。

因此，这两个代码片段中的数字含义如下：

0x10ec0257：代表 ALC257 音频编解码器的唯一标识符。
0x10ec：是 Realtek Semiconductor Corp. 的 PCI 厂商ID。
0x12f0：是一个特定硬件设备的 PCI Subsystem ID。

## PCI配置寄存器

- [Linux驱动之PCI子系统剖析](https://cloud.tencent.com/developer/article/2164590)

所有的PCI设备都有至少256字节的地址空间，其中前64字节是标准化的，被称为PCI配置寄存器，剩下的字节是设备相关的 (取决于具体的厂商，需要查看datasheet得知)。

PCI配置寄存器如下图所示。

![PCI配置寄存器](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/9178ba2599d6c4bb6c1f0f9a232cf3f3.png)

- Vendor ID: 标识硬件厂商，需要向特定组织进行注册。
- Device ID: 由硬件厂商来分配的设备ID，无需对ID进行注册。
- Subsystem ID、Subsystem Vendor ID: 用来进一步标识设备。

## 参考patch

- [ALSA: hda/realtek: Add quirk for another Asus K42JZ model](https://patchwork.kernel.org/project/alsa-devel/patch/20220805070331.13743-1-tangmeng@uniontech.com/)
- [ALSA: hda/realtek: Fix headset mic for Acer SF313-51](https://patchwork.kernel.org/project/alsa-devel/patch/20220711081527.6254-1-tangmeng@uniontech.com/)
