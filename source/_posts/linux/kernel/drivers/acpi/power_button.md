---
date: 2023/09/22 13:25:02
updated: 2023/09/25 12:55:17
---

# power_button

- [linux下获取按键响应事件](https://blog.csdn.net/jjjjjj0000/article/details/120187623)
- [linux下获取按键响应事件](https://www.cnblogs.com/yangwindsor/articles/3454955.html)

```bash
cd /sys/bus/acpi/drivers/button
```

```c
// drivers/acpi/button.c:398
static void acpi_button_notify(struct acpi_device *device, u32 event)
{
	struct acpi_button *button = acpi_driver_data(device); // 获取与按钮相关的数据结构

	struct input_dev *input; // 定义输入设备结构体指针
	int users; // 定义用户数

	switch (event) {
	case ACPI_FIXED_HARDWARE_EVENT:
		event = ACPI_BUTTON_NOTIFY_STATUS; // 重置事件为 ACPI_BUTTON_NOTIFY_STATUS
		/* fall through */
	case ACPI_BUTTON_NOTIFY_STATUS:
		input = button->input; // 获取按钮关联的输入设备结构体

		if (button->type == ACPI_BUTTON_TYPE_LID) { // 如果按钮类型是盖子开合按钮
			mutex_lock(&button->input->mutex); // 锁定输入设备的互斥锁
			users = button->input->users; // 获取输入设备的用户数
			mutex_unlock(&button->input->mutex); // 解锁输入设备的互斥锁

			if (users)
				acpi_lid_update_state(device, true); // 如果有用户使用输入设备，更新盖子状态
		} else { // 对于其他类型的按钮
			int keycode;

			acpi_pm_wakeup_event(&device->dev); // 唤醒事件
			if (button->suspended)
				break; // 如果按钮被挂起，则跳过后续操作

			keycode = test_bit(KEY_SLEEP, input->keybit) ? KEY_SLEEP : KEY_POWER; // 根据按钮类型确定按键代码
			input_report_key(input, keycode, 1); // 上报按键按下事件
			input_sync(input); // 同步输入事件
			input_report_key(input, keycode, 0); // 上报按键释放事件
			input_sync(input); // 同步输入事件

			acpi_bus_generate_netlink_event(
					device->pnp.device_class,
					dev_name(&device->dev),
					event, ++button->pushed); // 生成ACPI按钮事件的Netlink事件，递增按钮按下次数
		}
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event)); // 输出不支持的事件信息
		break;
	}
}
```

```c
// drivers/acpi/button.c:110
static struct acpi_driver acpi_button_driver = {
	.name = "button", // 驱动程序的名称为 "button"
	.class = ACPI_BUTTON_CLASS, // 驱动程序的类别为 ACPI_BUTTON_CLASS
	.ids = button_device_ids, // 驱动程序支持的设备标识列表
	.ops = {
		.add = acpi_button_add, // 添加设备时调用的回调函数
		.remove = acpi_button_remove, // 移除设备时调用的回调函数
		.notify = acpi_button_notify, // 处理设备通知事件的回调函数
	},
	.drv.pm = &acpi_button_pm, // 驱动程序的电源管理信息
};
```

```c
drivers/acpi/bus.c:450
acpi_device_install_notify_handler
```

----------------------

## 按下电源键后，acpi固件生成电源按钮事件的具体流程是啥？

按下电源键后，ACPI固件生成电源按钮事件的具体流程可以概括如下：

1. **电源按钮按下**：当用户按下电源按钮时，电源按钮硬件上的电路状态发生变化，通常是由于按钮的闭合。这个状态变化是电源按钮事件的触发。

2. **硬件检测**：ACPI固件定期轮询或等待电源按钮硬件的状态变化。一旦电源按钮按下，电源按钮硬件的状态变化会被检测到。

3. **事件检测**：ACPI固件检测到电源按钮事件的发生后，会识别事件的类型，即电源按钮事件。这一步通常涉及解析电源按钮事件的特定电路信号。

4. **生成ACPI事件**：一旦事件被检测到并识别，ACPI固件将生成一个ACPI事件，包含以下信息：

   - 事件类型：通常标识为电源按钮事件。
   - 事件参数：可能包括按钮按下的详细信息，如持续时间等。
   - 其他相关信息：与事件相关的附加数据。

5. **ACPI事件传递**：生成的ACPI事件需要传递给操作系统内核，以便内核可以进一步处理事件。这个传递通常通过ACPI中断或其他ACPI事件传递机制完成。

6. **ACPI中断处理程序**：ACPI中断处理程序位于操作系统内核中，负责处理ACPI事件。一旦事件传递给操作系统内核，ACPI中断处理程序会被触发。

7. **事件处理**：ACPI中断处理程序根据事件类型执行相应的操作。对于电源按钮事件，可能的操作包括：

   - 执行关机操作。
   - 执行休眠操作。
   - 执行其他电源管理操作。

总之，ACPI固件生成电源按钮事件的流程涉及硬件状态变化的检测、事件识别、生成ACPI事件、传递到操作系统内核以及在内核中的ACPI中断处理程序中进行事件处理。这种处理方式允许操作系统根据电源按钮事件的类型和配置来自定义响应。

-----------------------

## 第4步中的生成ACPI事件在硬件层面如何观测呢？

在硬件层面，ACPI固件观测电源按钮事件的方式通常涉及以下步骤：

1. **硬件电路设计**：在计算机系统的主板或电源管理控制器（例如，ACPI固件通常与主板上的BIOS或UEFI固件集成）上，存在一个与电源按钮相关的电路。这个电路通常包括与电源按钮物理连接的电线、电路元件和电子元器件。

2. **按钮状态检测**：硬件电路会不断检测与电源按钮连接的电线上的电压或电位差。电源按钮通常是一个开关，用户按下按钮时会导致电位差发生变化，从而被电路检测到。

3. **状态变化检测器**：电路中可能包括一个状态变化检测器或电平触发器，用于检测到电位差的变化。这个检测器可以识别电位差的升降沿（从高电位变为低电位或从低电位变为高电位）。

4. **电源按钮事件生成**：一旦状态变化检测器检测到电位差的变化（电源按钮按下），它会向ACPI固件发送一个信号或触发一个事件。这个事件通常是一个中断信号或一个电源按钮事件信号，通知ACPI固件有关电源按钮按下的发生。

5. **ACPI固件响应**：ACPI固件会响应收到的电源按钮事件信号，生成相应的ACPI事件。这个ACPI事件包含事件类型（通常是电源按钮事件）和相关的事件参数。

总之，硬件观测电源按钮事件的关键是通过电路设计和电路元件来检测电位差的变化，然后将这个变化转化为一个事件信号，通知ACPI固件有关事件的发生。这种硬件观测电源按钮事件的方式允许ACPI固件及时响应用户按下电源按钮的操作。

-----------------

## 中断或事件通知这里再细化一下，罗列出相关的函数?

中断或事件通知的具体实现会涉及到与硬件和操作系统相关的多个函数和组件。以下是涉及到中断或事件通知的一些相关函数和组件，尽管具体的函数和调用链可能会因Linux内核版本和系统配置而有所不同，但这些函数和组件通常是涉及到的：

1. **中断处理程序**：中断处理程序负责响应硬件产生的中断请求，其中可能包含与电源键事件相关的中断。这些函数通常在`arch/`和`drivers/`目录下实现，包括：

   - `irq_handler_t`：用于注册中断处理程序的函数指针类型。
   - `request_irq`：用于请求和注册中断处理程序的函数。
   - `handle_irq`：用于处理中断的通用中断处理程序。

2. **ACPI中断处理程序**：如果电源键事件与ACPI相关，ACPI中断处理程序可能会介入。这些函数通常位于`drivers/acpi`目录下，包括：

   - `acpi_ev_fixed_event_detect`：用于检测固定事件（如电源按钮事件）的ACPI中断处理程序。
   - `acpi_gbl_FADT`：包含固定事件的ACPI固定描述符表。
   - `acpi_ut_execute_HID`：用于执行与电源键事件相关的ACPI方法的函数。

3. **事件通知**：事件通知机制可以是中断、ACPI通知、信号等，通知操作系统有事件发生。涉及的函数和组件可能包括：

   - `acpi_ev_queue_notify_request`：用于将ACPI通知请求排队以触发事件。
   - `acpi_os_queue_for_execution`：将ACPI通知请求排队以在操作系统中执行。
   - Netlink通知机制：用于在Linux中将事件通知传递到用户空间。

4. **ACPI事件分发**：ACPI事件分发通常涉及到以下函数：

   - `acpi_bus_generate_netlink_event`：用于生成ACPI事件的Netlink通知，以通知用户空间或其他内核组件有关事件的发生。

5. **ACPI事件处理程序**：ACPI事件处理程序通常位于`drivers/acpi`目录下，用于根据事件类型执行相应的操作。对于电源键事件，可能包括：

   - `acpi_button_notify`：用于处理电源按钮事件的ACPI事件处理程序。

请注意，具体的代码路径和函数调用链可能因Linux内核版本和系统配置而有所不同。要深入了解特定版本的Linux内核实现，您可以查看相应版本的内核源代码。

```c
grep acpi_ev_fixed_event_detect . -inr --color --include={*.c,*.h}
./acpica/evsci.c:92:    interrupt_handled |= acpi_ev_fixed_event_detect();
./acpica/evevent.c:157: * FUNCTION:    acpi_ev_fixed_event_detect
./acpica/evevent.c:167:u32 acpi_ev_fixed_event_detect(void)
./acpica/acevents.h:34:u32 acpi_ev_fixed_event_detect(void);
```

```c
grep acpi_ev_install_sci_handler . -inr --color --include={*.h,*.c}
./acpica/evsci.c:140: * FUNCTION:    acpi_ev_install_sci_handler
./acpica/evsci.c:150:u32 acpi_ev_install_sci_handler(void)
./acpica/evevent.c:94:  status = acpi_ev_install_sci_handler();
./acpica/acevents.h:242:u32 acpi_ev_install_sci_handler(void);
```

```c
grep acpi_ev_install_xrupt_handlers . -inr --color --include={*.h,*.c}
./acpica/utxfinit.c:184:                status = acpi_ev_install_xrupt_handlers();
./acpica/evevent.c:70: * FUNCTION:    acpi_ev_install_xrupt_handlers
./acpica/evevent.c:80:acpi_status acpi_ev_install_xrupt_handlers(void)
./acpica/acevents.h:32:acpi_status acpi_ev_install_xrupt_handlers(void);
```

```c
grep acpi_enable_subsystem . -inr --color --include={*.h,*.c}
./acpica/utxfinit.c:100: * FUNCTION:    acpi_enable_subsystem
./acpica/utxfinit.c:110:acpi_status ACPI_INIT_FUNCTION acpi_enable_subsystem(u32 flags)
./acpica/utxfinit.c:114:        ACPI_FUNCTION_TRACE(acpi_enable_subsystem);
./acpica/utxfinit.c:194:ACPI_EXPORT_SYMBOL_INIT(acpi_enable_subsystem)
./acpica/evxfregn.c:35: * NOTE: This function should only be called after acpi_enable_subsystem has
./acpica/evxfregn.c:39: * initialized (via acpi_enable_subsystem.)
./bus.c:1107:   status = acpi_enable_subsystem(~ACPI_NO_ACPI_ENABLE);
./bus.c:1157:   status = acpi_enable_subsystem(ACPI_NO_ACPI_ENABLE);
```

```c
uname -a
Linux ARM-PC 5.10.0-arm64-desktop #1 SMP Thu Sep 21 17:42:23 CST 2023 aarch64 GNU/Linux

2023-09-22 14:08:30 ARM-PC kernel: [  180.165690] CPU: 0 PID: 1731 Comm: kworker/0:3 Tainted: G           OE     5.10.0-arm64-desktop #1
2023-09-22 14:08:30 ARM-PC kernel: [  180.174643] Hardware name: Centerm C7F-G3/C7F-G3, BIOS 1.05 20210816
2023-09-22 14:08:30 ARM-PC kernel: [  180.180993] Workqueue: kacpi_notify acpi_os_execute_deferred
2023-09-22 14:08:30 ARM-PC kernel: [  180.186642] Call trace:
2023-09-22 14:08:30 ARM-PC kernel: [  180.189080]  dump_backtrace+0x0/0x1e8
2023-09-22 14:08:30 ARM-PC kernel: [  180.192731]  show_stack+0x18/0x28
2023-09-22 14:08:30 ARM-PC kernel: [  180.196036]  dump_stack+0xf0/0x128
2023-09-22 14:08:30 ARM-PC kernel: [  180.199431]  acpi_button_notify+0x24/0x114 [button]
2023-09-22 14:08:30 ARM-PC kernel: [  180.204297]  acpi_device_notify+0x1c/0x28
2023-09-22 14:08:30 ARM-PC kernel: [  180.208296]  acpi_ev_notify_dispatch+0x60/0x70
2023-09-22 14:08:30 ARM-PC kernel: [  180.212728]  acpi_os_execute_deferred+0x1c/0x38
2023-09-22 14:08:30 ARM-PC kernel: [  180.217248]  process_one_work+0x1f4/0x4f0
2023-09-22 14:08:30 ARM-PC kernel: [  180.221247]  worker_thread+0x140/0x568
2023-09-22 14:08:30 ARM-PC kernel: [  180.224985]  kthread+0x110/0x118
2023-09-22 14:08:30 ARM-PC kernel: [  180.228201]  ret_from_fork+0x10/0x18
```

```c
../../../scripts/faddr2line evmisc.o acpi_ev_notify_dispatch+0x60/0x70
acpi_ev_notify_dispatch+0x60/0x70:
acpi_ev_notify_dispatch 于 /data3/home/yuanqiliang/code/arm-kernel-5.10/kernel/drivers/acpi/acpica/evmisc.c:184
```

```c
../../scripts/faddr2line bus.o acpi_device_notify+0x1c/0x28
acpi_device_notify+0x1c/0x28:
acpi_device_notify 于 /data3/home/yuanqiliang/code/arm-kernel-5.10/kernel/drivers/acpi/bus.c:425
```

系统启动后到图形化登录界面，按下电源键后，内核函数acpi_button_notify有被执行到。

系统登录后按下电源键后，内核函数acpi_button_notify没有被执行到。

这两个中情况下按下电源键，对于acpi固件来讲，区别在哪？acpi固件在第二种情况下到底捕捉到了acpi电源键按下事件没有？

第二次到底是ACPI事件通知机制有问题还是acpi固件没检测到电源按钮被按下？

第一次acpi固件检测到了电源键按下事件并成功通过ACPI事件通知机制将电源键按下事件上报给了内核，进一步导致acpi_button_notify函数被调用。

-----------------------------

## 电源按下后acpi固件电源按钮事件未能正确处理或未能在处理完后恢复正常状态，会不会导致再次按下电源键后acpi固件检测电源按下事件异常？

电源按下后，如果ACPI固件未能正确处理电源按钮事件或未能在处理完事件后恢复正常状态，可能会导致再次按下电源键时出现异常情况。这种异常情况可能包括以下情况之一：

1. **电源按钮事件丢失**：如果ACPI固件在首次按下电源按钮事件后未能正确处理并清除该事件，可能会导致后续的电源按钮事件被丢失或无法被正确检测。

2. **重复电源按钮事件**：ACPI固件可能会在一次电源按钮事件处理完成后意外地生成重复的事件。这可能会导致不必要的操作，例如多次关机或重启。

3. **系统状态不一致**：如果ACPI固件在电源按钮事件处理期间未能正确管理系统状态，可能会导致系统状态变得不一致，从而导致后续操作的异常行为。

要解决这些问题，通常需要进行以下操作：

- **更新ACPI固件**：检查计算机制造商是否提供了更新的BIOS或UEFI固件，其中可能包含了解决电源按钮事件处理问题的修复。

- **检查硬件**：确保电源按钮硬件没有故障。有时候，硬件故障可能导致电源按钮事件无法正确检测或处理。

- **操作系统和内核更新**：确保操作系统和内核是最新的，以确保其与ACPI固件协同工作正常。

- **ACPI设置**：在BIOS或UEFI设置中，检查与电源按钮事件相关的配置选项，确保其设置正确。

- **ACPI事件监测**：使用ACPI事件监测工具（如 `acpidump` 或 `acpi_listen`）来监视ACPI事件，以检查它们是否按预期生成。

- **系统状态恢复**：如果ACPI固件未能正确管理系统状态，可能需要手动干预来恢复系统状态。

如果您遇到了电源按钮事件处理的异常情况，建议首先尝试更新ACPI固件和操作系统，并检查硬件是否正常。如果问题仍然存在，可能需要更深入的系统诊断和调试，可能需要与计算机制造商或操作系统供应商联系以获取更多支持。
