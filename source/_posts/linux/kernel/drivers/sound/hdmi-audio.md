---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# hdmi-audio

217721 HDMI连接带有Audio 显示器，S3/S5/reboot唤醒听不到唤醒音

## 排查思路

- S3: 暂停到内存，唤醒播放提示音前pulseaudio存在

- S4: 暂停到硬盘，唤醒播放提示音前pulseaudio存在

- S5: 关机，播放提示音前pulseaudio存在

- reboot: 重新启动，启动时播放提示音前pulseaudio不存在

查看deepin-login-sound.service依赖:

```bash
systemctl list-dependencies deepin-login-sound.service
deepin-login-sound.service
● ├─system.slice
● ├─sound.target
● │ ├─alsa-restore.service
○ │ └─alsa-state.service
```

查看sound.target依赖:

```bash
systemctl list-dependencies sound.target
sound.target
● ├─alsa-restore.service
○ └─alsa-state.service
```

查看deepin-login-sound.service文件:

```bash
sudo systemctl cat deepin-login-sound.service
# /lib/systemd/system/deepin-login-sound.service
[Unit]
Description=Deepin login sound
Requires=sound.target
After=dbus.service lightdm.service

[Service]
Type=oneshot
ExecStart=/usr/lib/deepin-api/deepin-boot-sound.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

查看 /usr/lib/deepin-api/deepin-boot-sound.sh脚本

```bash
sudo cat /usr/lib/deepin-api/deepin-boot-sound.sh
#!/bin/sh

/usr/bin/dbus-send --system --print-reply --dest=com.deepin.api.SoundThemePlayer /com/deepin/api/SoundThemePlayer com.deepin.api.SoundThemePlayer.PlaySoundDesktopLogin&
```

## 问题

deepin-boot-sound.sh提示音的播放逻辑没问题，前后依赖关系没问题。

com.deepin.api.SoundThemePlayer这个走pulseaudio接口 or alsa接口？

应该走alsa接口，pulseauido在用户登录后才启动。

问题应该出在alsa-restore.service这个服务没有等到hdmi audio恢复到可用状态后再执行，还是cat /proc/asound/card*/eld\#* 的问题

## 解决方案

### /etc/udev/rules.d/69-hdmi-audio-check.rules

```bash
sudo tee /etc/udev/rules.d/69-hdmi-audio-check.rules <<EOF
# Run the HDMI audio check script before the alsa-restore.service
ACTION=="add", SUBSYSTEM=="sound", TAG+="systemd", ENV{SYSTEMD_WANTS}="check-hdmi-card.service", ENV{SYSTEMD_WANT_BEFORE}="alsa-restore.service"
EOF
```

```bash
sudo udevadm control --reload-rules
```

### /etc/systemd/system/check-hdmi-card.service

```bash
sudo tee /etc/systemd/system/check-hdmi-card.service <<EOF
[Unit]
Description=Check HDMI Audio Availability
After=sysinit.target
Before=alsa-restore.service

[Service]
Type=oneshot
ExecStart=/usr/bin/bash -c '/usr/lib/deepin-api/check-hdmi-card.sh'

[Install]
WantedBy=multi-user.target
EOF
```

### /usr/lib/deepin-api/check-hdmi-card.sh

```bash
sudo tee /usr/lib/deepin-api/check-hdmi-card.sh <<EOF
#!/bin/bash

# Check HDMI audio devices
hdmi_found=false

for eld_file in /proc/asound/card*/eld#*; do
    if [ -e "$eld_file" ]; then
        echo "HDMI audio device information is available in $eld_file:"
        cat "$eld_file"

        # Check the monitor_present and eld_valid flags
        monitor_present=$(grep "monitor_present" "$eld_file" | awk '{print $2}')
        eld_valid=$(grep "eld_valid" "$eld_file" | awk '{print $2}')

        if [ "$monitor_present" = "1" ] && [ "$eld_valid" = "1" ]; then
            hdmi_found=true
            # Perform additional actions as needed here
        fi
    else
        echo "No HDMI audio device found in $eld_file."
    fi
done

# Output the result
if [ "$hdmi_found" = true ]; then
    echo "HDMI audio device is ready and connected to an HDMI monitor."
else
    echo "No HDMI audio device found or not connected to an HDMI monitor."
fi
EOF
```

```bash
sudo chmod +x /usr/lib/deepin-api/check-hdmi-card.sh
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable check-hdmi-card.service
```

### 优缺点

优点：

- 耦合性大幅降低
- 播放时不必等待

缺点：

- 更改配置文件较多
