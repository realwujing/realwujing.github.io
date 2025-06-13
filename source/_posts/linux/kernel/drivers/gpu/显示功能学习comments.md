---
date: 2023/08/24 17:10:23
updated: 2023/08/24 17:10:23
---

# 显示功能学习comments

## 一、显示过程

### （一）开机过程中的显示变换

#### 1、固件logo到grub菜单

这个时候由固件UEFI提供的GOP驱动来支持显示，GOP驱动是由显卡厂商提供给固件厂商的，GOP驱动比较简单， 通常只有有限的分辨率，有限的像素深度。只是为了设置固件和显示grub菜单。

#### 2、grub菜单被选择之后到出现内核console日志

这个时候显示由内核efifb提供，efifib创建了/dev/fb0节点，但是并没有改变显示模式，只是沿用了固件GOP提供的Framebuffer。
efifb只是所有显卡的linux通用显卡驱动，等到后续显卡厂商编写的显卡驱动被加载后就调用remove_conflicting_framebuffers卸载掉之前加载的驱动。但是如果后续没有厂商显卡驱动加载的话，efifb会一直用，Xorg也会使用FBDEV扩展+/dev/fb0节点 去点亮桌面。一般来说，这时候Xorg只有一个分辨率。

#### 3、UOS logo到UOS桌面登陆界面

这个时候由厂商显卡驱动提供显示，如果没有厂商显卡驱动，则由内核efifb提供。主要是由plymouth调用drm接口去显示UOS logo。

#### 4、UOS桌面登陆界面输入密码后

这个时候显示由 Xorg 掌控，但是同时也受startdde配置文件影响。如果是第一次进入桌面，Xorg的默认分辨率是EDID提供的，一般EDID会将最大分辨率（原生分辨率）置为最佳分辨率并被Xorg用作默认分辨率。
部分linux显卡驱动，特别是虚拟显卡驱动，是无法获取显示器EDID信息的，会使用 drm_set_preferred_mode 设置一个最佳分辨率。
如果通过UOS控制中心重新设置过分辨率，那么startdde会将新分辨率保存为配置文件 ~/.config/deepin/startdde/display_v5.json。后续再次进入桌面，startdde会自动还原分辨率为上一次保存的分辨率。

### （二）应用程序的3D图形渲染过程

【这里描述的就是DRM框架，通常说的显卡驱动符合DRM框架，就是实现了这些功能。国内适配的显卡，有的做的不那么规范，有在umd里直接做ioctl的相当于绕过了drm】

#### 1、 应用程序到Mesa 3D

- 应用程序调用图形API库（如OpenGL）进行3D渲染。
- Mesa 3D库提供了OpenGL的实现，充当中间件，将这些命令转化为GPU特定的命令。

#### 2、 Mesa 3D到libdrm

- Mesa 3D库通过使用libdrm库中的接口与DRM子系统建立联系。如打开和关闭DRM设备文件（drmOpen、drmClose）、获取有关GPU设备的信息（drmModeGetResources）、帧缓冲管理（drmModeAddFB）、页面翻转（drmModePageFlip）、内存映射（drmModeMapDumb）等。

#### 3、 libdrm到DRM内核模块

- libdrm通过调用ioctl（Input/Output Control）函数，并传递相应的命令和参数，向DRM发送请求。

#### 4、 DRM到显示器和显示设备

- DRM子系统将执行的GPU命令发送到GPU Driver，GPU Driver解析命令并发送给GPU硬件。GEM会分配一部分显存用于存储渲染所需的图像数据，GPU会从显存中读取数据进行渲染，然后，GPU Driver会将渲染结果从显存复制到Framebuffer中。
- 应用程序与GPU Driver通信，请求访问渲染结果。应用程序发送绘制窗口请求到X Server/ Wayland Compositor，X Server/ Wayland Compositor使用ioctl发送请求到KMS以配置显示输出。
- KMS负责管理显示模式、分辨率等，确保显示器能正确显示图像，通过CRTC扫描framebuffer内容并编码好发送到显示器。

## 二、涉及到的组件介绍

### （一）用户空间

#### 1、窗口系统

如X Window System、Wayland。负责管理应用程序的窗口、处理用户输入事件、执行窗口管理任务（如最大化、最小化、移动窗口等）以及将应用程序的图形内容合成为整个屏幕上的用户界面。

#### 2、图形API库

如OpenGL。提供了一组函数和接口，允许应用程序发出图形渲染命令和创建图形对象。

#### 3、Mesa3D

开源的3D图形库，实现了OpenGL标准，提供了libGL.so ，允许应用程序通过OpenGL API与图形硬件进行通信，从而实现图形渲染和3D图形的显示。

#### 4、libdrm

对底层接⼝进⾏封装，向上层提供通⽤的API接⼝，主要是对各种IOCTL接⼝进⾏封装。提供了用户空间应用程序与DRM子系统通信的接口，包括打开DRM设备、查询GPU信息、创建DRM缓冲等。

![libdrm](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/libdrm.png)

libdrm.so是通⽤DRM⽀持的IOCTL操作；libdrm_xxxx.so是针对不同的硬件提供的特有IOCTL操作，实现硬件特有的功能。

一些函数：

- drmModeGetResources：用于获取DRM设备上的资源信息，如连接的显示器、可用的CRTC和连接器等。
- drmModeGetCRTC：获取CRTC的详细信息（如ID）。
- drmModeGetConnector：获取显示器连接器的详细信息。
- drmModeGetEncoder：获取编码器的详细信息。
- drmModeAddFB：将Buffer Object绑定到一个fb。

- Create_DUMB：【dumb buffer - 通常只用在kms中，dumb表示哑的、呆的，这里的意思是只在kms中使用，不保证在其它场景能用，这里理解为专用类似；dumb buffer和kms配合使用，创建硬件无关的用户空间程序，这里可理解为在用户空间kms使用dumb buffer这种方式，可以忽略底层硬件的差异】
- drmModeSetPlane：设置显卡的一个平面（plane）的输出。
- drmModeSetXX：

- drmModePageFlip：当Mesa 3D准备好显示新的图像时，可以使用此函数请求DRM系统在下一个垂直同步时切换显示，避免撕裂等问题。
- drmModeMapDumb/drmModeUnmapDumb：允许Mesa 3D映射和解除映射帧缓冲对象中的内存，以在内存中访问图像数据。

### （二）内核空间

#### 1、Framebuffer：帧缓冲对象、Plane：负责存储和组织渲染结果。

- Framebuffer：存储整个屏幕的图像，包括所有可见窗口、图形和文本等内容。
GPU将图形渲染的结果存储在帧缓冲对象中。

- Plane：允许将图像以不同的层叠方式组合在一起，每个Plane通常表示屏幕上的一个图层，例如视频、鼠标指针。

#### 2、KMS：主要功能为

- （1）更新画面：显示buffer的切换，多图层的合成方式，以及每个图层的显示位置。
- （2）设置显示参数：包括分辨率、刷新率、电源状态（休眠唤醒）等。

- 抽象模块：
  - CRTC：负责控制Framebuffer的显示输出。
  - Encoder：将CRTC的输出信号编码为显示器可以理解的格式。例如,将数字信号编码为VGA、HDMI或DisplayPort等模拟信号。
  - Connector：将Encoder输出的信号传输到显示器。常见的Connector包括VGA、DVI、HDMI和DisplayPort接口。

- 一些函数：
  - drm_mode_setplane
  - drm_mode_setcrtc

#### 3、 GEM：提供内存管理方法，主要负责显示buffer的分配和释放

一些函数：

- drm_mode_create_dumb_ioctl
- drm_mode_mmap_dumb_ioctl

## 三、关于测试

### 1、 modetest工具

由libdrm提供的测试程序，可以查询显示设备的支持状况，进行基本的显示测试，以及设置显示的模式。

（1）代码编译：
下载链接：https://dri.freedesktop.org/libdrm/libdrm-2.4.100.tar.bz2

解压并进入目录执行：

```bash
./configure
make -j4
```

编译完成后会在目录libdrm-2.4.100/tests/modetest下生成 modetest 可执行文件。

（2）modetest示例：

通过--help参数可以查看modetest支持的全部选项。

### 2、modeset：与显示有关的参数

（1）编辑 /etc/default/grub

![grub](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/grub.png)

（2）“quiet splash video=LVDS-1:1920×1080"：设置分辨率为1920×1080，假定显示设备标识为LVDS-1。

（3）更新grub配置：sudo update-grub，重启生效

### 3、nomodeset：禁用内核模式设置

在某些情况下，禁用KMS可以解决启动问题或出现兼容性问题时恢复显示。

### 4、显示设备的特性

分辨率、刷新率、颜色深度、亮度和对比度、响应时间、连接接口、视角、旋转和调整、自适应同步技术、多显示器支持、电源管理和节能模式、色温调整、色域覆盖率、音频支持、触摸屏和多点触控

## 四、对一些问题的思考

### 1、为什么需要Mesa 3D？

Mesa 3D库的实现提供了对图形硬件的抽象，使得应用程序可以在不同的图形硬件设备上运行，而不需要关心底层硬件的细节。它提供了一致的编程接口，使得应用程序可以跨平台地使用OpenGL功能，无论是在Windows、Linux还是其他操作系统上。 

【mesa 3D并不是必需的，可以也存在其它opengl的实现。mesa3D用的普遍，一方面是开源免费，另一方面是功能多、而且社区活跃。mesa3D除了提供了Opengl的实现，还将GPU计算、开源的A卡、Intel显卡、开源N卡的umd包含在内，通知支持vulkan接口】

### 2、为什么需要libdrm？

不仅避免了将内核接口直接暴露给应用程序，而且还表现出在程序之间重用和共享代码的优势。
【libdrm的核心或者说设计的初衷的确是将drm接口暴露给用户层。在上面mesa3D的功能中提到了，开源的显卡umd在Mesa3D范围内，libdrm层也做了显卡相关的扩展（类似libdrm-intel）, libdrm层逻辑上分为dri接口和kms接口】

## 五、一些疑惑的地方

1、作为测试人员，可测的地方有哪些，怎样有针对性地对显示功能进行测试？

2、在X Window System/Wayland中，应用程序的2D图形显示过程是怎样的？不需要Mesa 3D库和内核中的DRM参与吗？

我的理解是否正确：在X Window System中，2D图形是由DDX支持的，DDX直接与libdrm交互。在Wayland中，不论是2D还是3D，都通过Mesa 3D库与libdrm进行交互。

3、不太明白DRM Driver和GPU Driver的具体含义，GPU Driver是特定的显卡驱动程序吗？比如intel、radeon、nouveau。GPU Driver的作用是解析命令并发送给GPU硬件并将渲染结果从显存复制到Framebuffer？DRM Driver的作用也是如此吗？[DRM driver是DRM框架的实现，GPU driver是将显卡在DRM框架里实现的程序；DRM driver是抽象出来的公共操作，GPU driver是显卡独有的；类似Xserver、DDX的关系]

4、在应用程序的3D图形渲染过程的第4步的第二小步中：X Server/ Wayland Compositor使用ioctl发送请求到KMS，这里的ioctl与libdrm无关吗？窗口合成工作是在set mode后合成吗？

5、在应用程序的3D图形渲染过程中，GEM具体的工作是什么？只知道是负责管理显示buffer的分配和释放，但在整个流程中无法说清GEM的作用【所有操作针对的都是数据，数据的管理就是buffer的管理】。

6、modetest找不到设备：【modeset需要将当前图形杀掉】

![modeset](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/modeset.png)
