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

drivers/acpi/bus.c:450
acpi_device_install_notify_handler

-------------------------------------------

按下电源键会写相关寄存器吗？

按下电源键通常不会直接导致写入相关寄存器。电源键事件的处理通常涉及到ACPI（高级配置与电源管理接口）子系统，该子系统会接收电源键事件并触发相应的处理。电源键事件处理过程通常包括以下步骤：

1. **检测电源键事件**：当用户按下电源键时，电源管理硬件（通常是主板或固件）会检测到电源键的状态变化。这个状态变化通常不会导致直接的寄存器写入，而是会生成一个ACPI事件。

2. **生成ACPI事件**：电源管理硬件会生成一个ACPI事件，指示电源键的状态变化。这个事件会传递到ACPI子系统中。

3. **ACPI事件处理**：ACPI子系统会根据接收到的事件类型（电源按钮事件）调用相应的事件处理程序。这个事件处理程序通常会执行与电源按钮事件相关的操作，例如执行关机、休眠或其他电源管理操作。

4. **系统操作**：事件处理程序可能会请求操作系统执行特定的系统操作，例如关机或休眠。这些系统操作通常由操作系统内部的功能来实现，而不是通过寄存器写入。

总之，按下电源键通常会生成一个ACPI事件，然后由ACPI子系统处理，而不是直接导致对寄存器的写入。电源键事件处理程序会根据事件类型执行相应的操作，但这些操作通常是通过操作系统内部的机制来实现的，而不是通过写入寄存器来完成。寄存器的写入通常用于硬件配置和控制，而不是用于处理电源键事件。


-------------------


中断或事件通知这里再细化一下，罗列出相关的函数?

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


grep acpi_ev_fixed_event_detect . -inr --color --include={*.c,*.h}
./acpica/evsci.c:92:    interrupt_handled |= acpi_ev_fixed_event_detect();
./acpica/evevent.c:157: * FUNCTION:    acpi_ev_fixed_event_detect
./acpica/evevent.c:167:u32 acpi_ev_fixed_event_detect(void)
./acpica/acevents.h:34:u32 acpi_ev_fixed_event_detect(void);


grep acpi_ev_install_sci_handler . -inr --color --include={*.h,*.c}
./acpica/evsci.c:140: * FUNCTION:    acpi_ev_install_sci_handler
./acpica/evsci.c:150:u32 acpi_ev_install_sci_handler(void)
./acpica/evevent.c:94:  status = acpi_ev_install_sci_handler();
./acpica/acevents.h:242:u32 acpi_ev_install_sci_handler(void);



grep acpi_ev_install_xrupt_handlers . -inr --color --include={*.h,*.c}
./acpica/utxfinit.c:184:                status = acpi_ev_install_xrupt_handlers();
./acpica/evevent.c:70: * FUNCTION:    acpi_ev_install_xrupt_handlers
./acpica/evevent.c:80:acpi_status acpi_ev_install_xrupt_handlers(void)
./acpica/acevents.h:32:acpi_status acpi_ev_install_xrupt_handlers(void);



grep acpi_enable_subsystem . -inr --color --include={*.h,*.c}
./acpica/utxfinit.c:100: * FUNCTION:    acpi_enable_subsystem
./acpica/utxfinit.c:110:acpi_status ACPI_INIT_FUNCTION acpi_enable_subsystem(u32 flags)
./acpica/utxfinit.c:114:        ACPI_FUNCTION_TRACE(acpi_enable_subsystem);
./acpica/utxfinit.c:194:ACPI_EXPORT_SYMBOL_INIT(acpi_enable_subsystem)
./acpica/evxfregn.c:35: * NOTE: This function should only be called after acpi_enable_subsystem has
./acpica/evxfregn.c:39: * initialized (via acpi_enable_subsystem.)
./bus.c:1107:   status = acpi_enable_subsystem(~ACPI_NO_ACPI_ENABLE);
./bus.c:1157:   status = acpi_enable_subsystem(ACPI_NO_ACPI_ENABLE);



