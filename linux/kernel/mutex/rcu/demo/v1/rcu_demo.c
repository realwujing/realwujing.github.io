#include <linux/module.h>   // 内核模块相关头文件
#include <linux/kernel.h>   // 内核日志相关头文件
#include <linux/init.h>     // 模块初始化和退出相关头文件
#include <linux/rcupdate.h> // RCU（Read-Copy-Update）相关头文件
#include <linux/slab.h>     // 内存分配相关头文件

// 定义一个简单的共享数据结构
struct my_data
{
    int value; // 数据字段
};

// 全局共享数据指针，受 RCU 保护
struct my_data __rcu *global_data;

static int __init rcu_demo_init(void)
{
    struct my_data *new_data; // 定义指向新数据的指针

    printk(KERN_INFO "RCU Demo: Initializing\n"); // 打印初始化信息

    // 分配初始数据
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL); // 分配内存
    if (!new_data)                                     // 检查内存分配是否成功
        return -ENOMEM;                                // 返回内存不足错误

    new_data->value = 42; // 初始化数据值

    // 初始化 global_data
    rcu_assign_pointer(global_data, new_data); // 使用 RCU 安全地初始化全局指针

    // 演示读操作
    rcu_read_lock(); // 开始 RCU 读操作
    {
        struct my_data *data = rcu_dereference(global_data);          // 获取 RCU 保护的指针
        printk(KERN_INFO "RCU Demo: Read value = %d\n", data->value); // 打印读取的值
    }
    rcu_read_unlock(); // 结束 RCU 读操作

    // 演示写操作（更新数据）
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL); // 分配新的数据内存
    if (!new_data)                                     // 检查内存分配是否成功
        return -ENOMEM;                                // 返回内存不足错误

    new_data->value = 100; // 设置新数据的值

    // 使用 RCU 安全地更新指针
    struct my_data *old_data = rcu_dereference_protected(global_data, 1); // 获取旧数据指针
    rcu_assign_pointer(global_data, new_data);                            // 更新全局指针为新数据

    // 等待所有读者完成
    synchronize_rcu(); // 等待所有正在进行的 RCU 读操作完成

    // 释放旧数据
    kfree(old_data); // 释放旧数据的内存

    // 再次读取以验证更新
    rcu_read_lock(); // 开始 RCU 读操作
    {
        struct my_data *data = rcu_dereference(global_data);             // 获取 RCU 保护的指针
        printk(KERN_INFO "RCU Demo: Updated value = %d\n", data->value); // 打印更新后的值
    }
    rcu_read_unlock(); // 结束 RCU 读操作

    return 0; // 返回成功
}

static void __exit rcu_demo_exit(void)
{
    struct my_data *data; // 定义指向数据的指针

    printk(KERN_INFO "RCU Demo: Exiting\n"); // 打印退出信息

    // 清理
    data = rcu_dereference_protected(global_data, 1); // 获取当前全局数据指针
    rcu_assign_pointer(global_data, NULL);            // 将全局指针设置为 NULL

    synchronize_rcu(); // 等待所有正在进行的 RCU 读操作完成
    kfree(data);       // 释放数据的内存
}

module_init(rcu_demo_init); // 指定模块初始化函数
module_exit(rcu_demo_exit); // 指定模块退出函数

MODULE_LICENSE("GPL");                                // 设置模块许可证
MODULE_AUTHOR("Grok");                                // 设置模块作者
MODULE_DESCRIPTION("Simple RCU Demo for Linux 5.10"); // 设置模块描述