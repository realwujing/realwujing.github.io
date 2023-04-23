---
date: 2023/04/21 15:56:04
updated: 2023/04/21 15:56:04
---

# fluid主题自定义标签页小图标

## 制作favicon图标

准备好用作标签页/网页小图标favicon的图片。

在线[制作ico图标](https://www.bitbug.net/)。

推荐16*16、32*32。

将下载后的图标复制到`source/images`目录下。

## 配置_config.fluid.yml

增加如下配置：

```text
# 用于浏览器标签的图标
# Icon for browser tab
favicon: /img/favicon.png

# 用于苹果设备的图标
# Icon for Apple touch
apple_touch_icon: /img/favicon.png
```

## More

- [https://github.com/Cenergy/blog-dev/blob/dev/_config.fluid.yml](https://github.com/Cenergy/blog-dev/blob/dev/_config.fluid.yml)
- [Hexo攻略-更换网页图标](https://blog.csdn.net/qq_39181839/article/details/109477357)