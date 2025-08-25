---
date: 2023/11/15 10:58:15
updated: 2023/11/15 10:58:15
---

# grub

## grub 启动项配置文件示例

```bash
menuentry 'UOS Desktop 20 Pro GNU/Linux' --class uos --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-2065c821-2f8f-4182-94ac-fb190c209054' {
        set root='hd0,gpt2'
        if [ x$feature_platform_search_hint = xy ]; then
          search --no-floppy --fs-uuid --set=root --hint-ieee1275='ieee1275//disk@0,gpt2' --hint-bios=hd0,gpt2 --hint-efi=hd0,gpt2 --hint-baremetal=ahci0,gpt2  86213e46-ff4e-41db-a56a-ff2c8841fc1e
        else
          search --no-floppy --fs-uuid --set=root 86213e46-ff4e-41db-a56a-ff2c8841fc1e
        fi
        linux   /vmlinuz-5.10.0-loongson-3-desktop root=UUID=2065c821-2f8f-4182-94ac-fb190c209054 ro  video=efifb:nobgrt splash quiet console=tty loglevel=0 ima_appraise=off libahci.ignore_sss=1
        initrd  /initrd.img-5.10.0-loongson-3-desktop
        boot 
}
```

这段代码是 GRUB（GRand Unified Bootloader）引导加载程序的配置文件（通常是 /boot/grub/grub.cfg 或类似位置）中的一部分。让我为你解释其中的一些关键行：

```bash
menuentry 'UOS Desktop 20 Pro GNU/Linux' --class uos --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-2065c821-2f8f-4182-94ac-fb190c209054' {
```

- menuentry 'UOS Desktop 20 Pro GNU/Linux': 定义引导菜单项的标题为 'UOS Desktop 20 Pro GNU/Linux'。

- --class uos --class gnu-linux --class gnu --class os: 为菜单项设置类别，包括 uos、gnu-linux、gnu 和 os。这些类别可以用于定义 GRUB 主题和其他显示效果。

- $menuentry_id_option 'gnulinux-simple-2065c821-2f8f-4182-94ac-fb190c209054': 为菜单项设置一个唯一的 ID，这个 ID 通常基于操作系统的 UUID 或其他标识符。这有助于区分不同的操作系统。

```bash
set root='hd0,gpt2'
```

- 这行命令设置了 GRUB 根文件系统的位置。在这里，hd0 表示第一个硬盘（通常是 /dev/sda），gpt2 表示硬盘上的 GPT（GUID Partition Table）分区号为 2。

```bash
if [ x$feature_platform_search_hint = xy ]; then
  search --no-floppy --fs-uuid --set=root --hint-ieee1275='ieee1275//disk@0,gpt2' --hint-bios=hd0,gpt2 --hint-efi=hd0,gpt2 --hint-baremetal=ahci0,gpt2  08822860-675a-4e7d-b597-8093d8230142
else
  search --no-floppy --fs-uuid --set=root 08822860-675a-4e7d-b597-8093d8230142
fi
```

- 这是一个条件语句，根据 GRUB 的特性选择不同的搜索方式。如果 feature_platform_search_hint 存在（xy 表示不为空），则使用具有不同搜索提示的 search 命令。否则，使用标准的 search 命令。如果支持的话，会根据平台特定的提示（hints）来搜索设备。
- 这行命令设置了 GRUB 所在分区文件系统的 UUID。

- 在 grub 中，search 命令的具体实现取决于所使用的引导方式（BIOS 或 UEFI）以及文件系统类型等因素。通常情况下，grub 会在超级块（Superblock）中搜索文件系统的 UUID。

- 超级块是文件系统的元数据结构之一，包含了文件系统的关键信息，例如文件系统类型、块大小、inode 信息等。文件系统的 UUID 通常也存储在超级块中。

- 在 grub 的 search 命令中，--fs-uuid 选项指定了要根据文件系统 UUID 进行搜索。这表示 grub 会在指定的设备上搜索具有特定 UUID 的文件系统。

- 总体而言，grub 的 search 命令不是直接在超级块中搜索 UUID，而是通过读取文件系统的元数据结构（包括超级块）来找到具有指定 UUID 的文件系统。这使得 grub 能够正确识别和定位根文件系统，而无需依赖特定的设备路径。

```bash
linux   /vmlinuz-5.10.0-loongson-3-desktop root=UUID=2065c821-2f8f-4182-94ac-fb190c209054 ro  video=efifb:nobgrt splash quiet console=tty loglevel=0 ima_appraise=off libahci.ignore_sss=1
```

- 这是加载 Linux 内核的命令。它指定了内核的路径（/vmlinux-5.10.197），以及一些内核启动参数，如 root 指定了根文件系统的 UUID，ro 表示以只读模式挂载根文件系统，splash 表示启用启动时的画面，console=ttyS1,115200 表示将控制台输出重定向到串口 ttyS1（波特率为 115200），loglevel=8 表示设置内核消息的日志级别为 8，initcall_debug 和 ima_appraise=off 等是其他内核参数。

```bash
initrd /initrd.img-5.10.197
```

- 这是加载 initramfs（初始内存文件系统）的命令，它位于 /initrd.img-5.10.197。

```bash
boot
```

- 这是最终的启动命令，表示执行之前的配置，启动操作系统。

总体而言，这是一个典型的 GRUB 配置文件段，用于加载 Linux 内核和 initramfs 并启动操作系统。

## root=UUID 与 root=/dev/sda3的区别

### root=UUID

当使用 root=UUID=<uuid> 这样的引导参数时，Linux 内核在启动过程中通过以下步骤来识别这个参数并挂载真正的根文件系统：

- 内核启动： 引导加载程序加载内核映像（vmlinuz）到内存中，同时传递引导参数。其中包括 root=UUID=<uuid>，其中 <uuid> 是根文件系统的唯一标识符。

- 内核初始化： 内核初始化过程中，它读取引导加载程序传递的命令行参数，并解析 root=UUID=<uuid>，提取出 <uuid>。

- 设备发现： 内核开始进行设备发现，探测和初始化硬件设备，包括存储设备。这个过程中，内核可能会获得硬盘和分区的基本信息。

- UUID 匹配： 内核使用之前提取的 <uuid> 与硬盘上的分区进行匹配。内核会检查硬盘上每个分区的文件系统 UUID，与指定的 <uuid> 进行比较。

- 挂载根文件系统： 一旦找到匹配的分区，内核会尝试挂载这个分区作为根文件系统。这个过程涉及到文件系统的识别和挂载操作。内核会尝试识别根文件系统的文件系统类型，并调用相应的文件系统驱动程序进行挂载。

- 切换根文件系统： 一旦成功挂载根文件系统，内核会切换到新的根文件系统，并启动用户空间的初始化进程（通常是 /sbin/init）。

使用 UUID 来标识根文件系统具有优势，因为它不受设备路径变化的影响。设备路径可能会随着硬件配置的变化而改变，而 UUID 作为唯一标识符，更加稳定地确保内核能够找到正确的根文件系统。

### root=/dev/sda3

在 Linux 内核启动的过程中，内核通过一系列步骤找到和挂载根文件系统。指定的 root=/dev/sda3 参数告诉内核使用哪个块设备作为根文件系统，但是这个信息并不是直接从磁盘上读取的。以下是内核启动时找到根文件系统的基本步骤：

- 引导加载程序（Bootloader）： 在系统启动时，引导加载程序（例如 GRUB）负责加载内核映像（vmlinuz）和初始内存盘（initramfs）到内存中。引导加载程序的配置文件中通常包含了 root= 参数，指定了根文件系统的设备。

- 内核初始化： 内核初始化过程中，内核会读取引导加载程序传递的命令行参数，其中包括 root= 参数，即指定的根文件系统设备。这个信息被内核保存在相应的内部数据结构中。

- 设备发现： 内核开始进行设备发现，探测和初始化硬件设备。这包括存储设备，例如硬盘和分区。

- 块设备识别： 内核会识别块设备，包括磁盘和分区。在这个阶段，内核使用之前保存的 root= 参数指定的块设备路径（例如 /dev/sda3）来确定根文件系统所在的设备。

- 挂载根文件系统： 一旦找到了指定的块设备，内核会尝试挂载根文件系统。这涉及到文件系统的识别和挂载操作。内核会尝试识别根文件系统的文件系统类型，并调用相应的文件系统驱动程序进行挂载。

- 切换根文件系统： 一旦成功挂载根文件系统，内核会切换到新的根文件系统，并启动用户空间的初始化进程（通常是 /sbin/init）。

这个过程中，指定的 root= 参数提供了一个初始的块设备路径信息，帮助内核找到根文件系统。然而，在现代系统中，使用 UUID 或者标签等更为稳健的方式来指定根文件系统是更为推荐的，因为这可以避免在设备路径发生变化时引起的问题。这种情况下，内核会通过设备的唯一标识符来找到并挂载根文件系统。

#### 设备发现

在第3步中，设备发现是指内核开始对系统中的硬件设备进行探测和初始化。这个过程包括对存储设备（如硬盘和分区）的探测。在设备发现阶段，内核会识别硬件设备，并建立与这些设备的连接，以便在接下来的引导过程中使用。

具体地说，设备发现的过程中，内核会执行以下操作：

- 加载硬件驱动程序： 内核会加载适当的硬件驱动程序，这些驱动程序负责与硬件通信和管理。

- PCI 设备探测： 内核会扫描 PCI 总线上的设备，识别连接到系统的各种设备，包括存储控制器等。

- ATA/SATA 设备探测： 内核会检测并初始化与 ATA/SATA 接口连接的硬盘设备。

- USB 设备探测： 如果系统中存在 USB 接口，内核也会检测和初始化连接到 USB 总线上的设备，其中可能包括外部硬盘等存储设备。

在设备发现的过程中，内核可能会获得有关硬盘和分区的基本信息，但这些信息通常是内核用于后续步骤的基础，如挂载根文件系统。在设备发现阶段，具体的 /dev/sda3 这样的设备节点并没有被创建。这些设备节点是由用户空间的 udev 服务在后续阶段动态生成的。

#### 块设备识别

在第4步中，块设备识别是指内核对块设备（通常是硬盘、分区等）进行识别和初始化的过程。这个过程涉及到内核的块设备驱动程序，以及硬件检测和识别的一系列操作。以下是这个步骤的一些详细内容：

- 块设备驱动程序加载： 内核在启动时会加载与块设备通信的驱动程序，例如 SATA、SCSI 等。这些驱动程序负责与相应的硬件通信，管理块设备的读写等操作。

- PCI 设备扫描： 内核扫描 PCI 总线上的设备，包括存储控制器。如果有存储控制器，内核会尝试初始化并加载相应的驱动程序。

- ATA/SATA 设备初始化： 内核会检测连接到 ATA/SATA 接口的硬盘设备。这个过程可能包括对硬盘的识别、参数设置和初始化。

- 设备节点分配： 一旦块设备被成功识别和初始化，内核为这些块设备分配相应的设备节点。这些设备节点通常以 /dev/sdX 的形式表示，其中 X 是字母，代表不同的块设备。

- 设备路径解析： 如果在引导参数中指定了根文件系统的设备路径（例如 root=/dev/sda3），内核会根据这个信息来找到相应的块设备。在这个阶段，内核建立了硬件设备与用户空间的设备节点之间的关联。

- 分区表解析： 内核读取块设备上的分区表，识别硬盘上的分区信息。这包括了每个分区的起始位置、大小等信息。

总的来说，块设备识别阶段是内核在引导过程中识别和初始化硬盘、分区等块设备的过程。这个阶段的成功执行是确保内核能够找到并正确挂载根文件系统的关键步骤。

#### 为啥有的时候写成root=/dev/sd3才能启动？

至于为什么有时候需要写成 root=/dev/sdX 才能启动，可能是由于以下原因：

GRUB 配置问题： GRUB 配置文件中的设备路径可能不正确，或者 GRUB 对 UUID 的支持有问题。

initramfs 配置问题： initramfs 中可能没有包含正确的文件系统模块，导致无法识别 UUID。

引导加载程序问题： 引导加载程序可能对某些硬件配置或文件系统特性支持不完善，需要通过设备路径来确保正确的引导。

在实际使用中，使用 UUID 通常是更可靠和推荐的方式，因为它减少了对硬件配置的依赖。如果存在问题，建议检查 GRUB 配置、initramfs 配置以及引导加载程序的相关问题。

## grub stage1 stage2 stage3 的区别

GRUB（GRand Unified Bootloader）的引导过程分为三个阶段，分别是 Stage 1、Stage 1.5 和 Stage 2。每个阶段有不同的功能和作用。

Stage 1:

- Stage 1 是 GRUB 引导加载程序的第一阶段，通常存储在引导扇区（Master Boot Record，MBR）的开头。其主要任务是加载 Stage 1.5 或 Stage 2。

Stage 1.5:

- Stage 1.5 是一个过渡阶段，通常存储在分区的起始扇区。由于 MBR 的空间有限，Stage 1 负责加载 Stage 1.5，以便在 Stage 1.5 中可以更多地加载引导信息和模块。Stage 1.5 的存在使得 GRUB 能够处理更多的文件系统和分区格式，而不受 MBR 大小的限制。

Stage 2:

- Stage 2 是 GRUB 的主要引导加载程序，包含在文件系统中的特定位置。一旦 Stage 1 或 Stage 1.5 完成其任务，它们就会加载 Stage 2。Stage 2 负责读取 GRUB 的配置文件（通常是 grub.cfg）和操作系统内核，然后引导操作系统。

这种分层结构允许 GRUB 适应不同的硬件配置和文件系统，使其更加灵活。Stage 1 和 Stage 1.5 的目标是足够小而简单，以适应引导扇区的有限空间，而 Stage 2 则更加复杂，负责处理更多的引导任务。

需要注意的是，随着计算机硬件和引导标准的发展，UEFI（Unified Extensible Firmware Interface）逐渐取代了传统的 BIOS，并引入了新的引导机制。UEFI 不再需要 Stage 1.5，并且整个引导过程更加灵活。

## UEFI 不再需要 Stage 1.5

在传统的 BIOS 引导中，Stage 1.5 的存在是为了弥补 BIOS 的限制，因为 MBR（Master Boot Record）的空间有限，难以容纳足够的代码和数据。Stage 1.5 被设计为一个额外的引导加载程序，存储在分区的起始扇区，允许加载更多的引导信息和模块。

UEFI（Unified Extensible Firmware Interface）引入了一种不同的引导机制，与传统的 BIOS 引导方式有很大的区别。在 UEFI 引导中，整个引导过程更加灵活和模块化，不再受到 MBR 大小的限制。以下是一些与 UEFI 引导相关的特点：

- GPT 分区表： UEFI 引导使用 GPT（GUID Partition Table）分区表而不是 MBR，提供更大的分区和更灵活的磁盘布局。

- EFI系统分区（ESP）： UEFI 引导系统通常有一个特定的分区，称为 EFI 系统分区（ESP），其中包含引导加载器、内核文件和其他引导信息。ESP 中的文件系统通常是 FAT32。

- UEFI固件中的引导管理器： UEFI 固件内置了一个引导管理器，称为 UEFI Boot Manager。UEFI Boot Manager 可以直接从 ESP 中的可执行文件引导操作系统。

- EFI加载器： 引导加载器（Boot Loader）在 UEFI 中被称为 EFI 加载器。这些加载器通常是 EFI 可执行文件，存储在 ESP 中，并由 UEFI Boot Manager 加载。

由于这些特性，UEFI 不再需要 Stage 1.5 的过渡阶段，因为整个引导过程更加灵活且不受 MBR 大小限制。UEFI 引导可以直接从 EFI 系统分区加载操作系统的 EFI 加载器，而不需要像传统 BIOS 引导那样分为多个阶段。这使得 UEFI 引导更加现代化和适应性强。

## EFI加载器

EFI（Extensible Firmware Interface）加载器是 UEFI 引导过程中的一个关键组件，负责加载和启动操作系统。EFI 加载器是一个 EFI 可执行文件，通常存储在 EFI 系统分区（ESP）中，其路径和文件名可以在 UEFI Boot Manager 中进行配置。

以下是 EFI 加载器的一些关键特点和概念：

- 文件路径： EFI 加载器的路径通常以 EFI 系统分区为根，然后按照一定的规范组织。例如，一个典型的 EFI 加载器路径可能是 /EFI/{vendor}/{loader_name}.efi，其中 {vendor} 是厂商或发行版的名称，{loader_name} 是加载器的名称。

- EFI System Partition（ESP）： ESP 是一个专用的分区，用于存储 EFI 加载器、引导配置文件以及其他引导相关的文件。ESP 使用 FAT32 文件系统，因为 FAT32 在多个操作系统之间具有广泛的兼容性。

- 引导配置文件： EFI 加载器通常伴随一个引导配置文件，这个文件包含了引导加载器的配置信息，例如指定操作系统内核的路径、启动参数等。这个引导配置文件的常见名称是 bootx64.efi。

- UEFI Boot Manager： UEFI Boot Manager 是 UEFI 固件内置的引导管理器，负责在系统启动时选择并加载操作系统。Boot Manager 通过查找 ESP 中的 EFI 加载器和引导配置文件来构建引导菜单。

- 加载过程： 当计算机启动时，UEFI Boot Manager 会扫描 ESP，找到 EFI 加载器的路径和引导配置文件。然后，Boot Manager 将控制权传递给 EFI 加载器，EFI 加载器负责加载操作系统内核并传递启动参数。

- 多操作系统支持： EFI 加载器的设计允许支持多个操作系统。每个操作系统都有自己的 EFI 加载器，它们被存储在 ESP 的不同路径下，并通过引导配置文件进行配置。

总体而言，EFI 加载器是 UEFI 引导过程中的核心组件之一，它使得操作系统的引导过程更加灵活和现代化。这种灵活性和标准化的设计有助于支持多引导配置和多操作系统环境。

## More

- [Linux系统启动管理](https://realwujing.github.io/linux/kernel/kernel/#%E5%90%AF%E5%8A%A8%E7%AE%A1%E7%90%86)

### bugs

- <https://bugs.launchpad.net/oem-priority/+bug/1842320>
