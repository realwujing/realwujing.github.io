# mihomo use guide

- <https://www.clashverge.dev/faq/linux.html>
- <https://github.com/MetaCubeX/mihomo/releases>
- <https://github.com/MetaCubeX/mihomo/releases/tag/v1.19.11>

## 安装mihomo

```bash
wget https://github.com/MetaCubeX/mihomo/releases/download/v1.19.11/mihomo-linux-arm64-v1.19.11.deb
```

```bash
sudo apt install ./mihomo-linux-arm64-v1.19.11.deb
```

## 配置mihomo

```bash
mkdir -p ~/.config/mihomo
```

```bash
wget https://realwujing.github.io/linux/kernel/net/proxy/mihomo/config.yaml -O ~/.config/mihomo/config.yaml
```

本config.yaml参照下方链接修改，加入了github走代理，默认走美国节点、更换健康检查URL为google、去掉特殊字符等。

- <https://github.com/yyhhyyyyyy/selfproxy/blob/main/Mihomo/mihomo_single.yaml>

## 启动 mihomo

```bash
sudo mihomo -d ~/.config/mihomo
```
