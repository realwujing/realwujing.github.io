#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

// 定义共享数据结构
struct my_data
{
    int value;
};

// 全局共享数据指针，受 RCU 保护
struct my_data __rcu *global_data;

// 控制线程运行的标志
static int running = 1;

// 读者线程函数
static int reader_thread(void *arg)
{
    int id = *(int *)arg;

    while (running)
    {
        rcu_read_lock();
        {
            struct my_data *data = rcu_dereference(global_data);
            printk(KERN_INFO "Reader %d: Read value = %d\n", id, data->value);
        }
        rcu_read_unlock();
        msleep(500); // 每 500ms 读取一次
    }

    printk(KERN_INFO "Reader %d: Exiting\n", id);
    return 0;
}

// 写者线程函数
static int writer_thread(void *arg)
{
    int new_value = 100;

    while (running)
    {
        struct my_data *new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
        if (!new_data)
        {
            printk(KERN_ERR "Writer: kmalloc failed\n");
            msleep(1000);
            continue;
        }

        new_data->value = new_value++;

        // 使用 RCU 更新指针
        struct my_data *old_data = rcu_dereference_protected(global_data, 1);
        rcu_assign_pointer(global_data, new_data);

        printk(KERN_INFO "Writer: Updated value to %d\n", new_data->value);

        // 等待所有读者完成
        synchronize_rcu();
        kfree(old_data);

        msleep(2000); // 每 2s 更新一次
    }

    printk(KERN_INFO "Writer: Exiting\n");
    return 0;
}

static struct task_struct *reader1, *reader2, *writer;
static int reader1_id = 1, reader2_id = 2;

static int __init rcu_demo_init(void)
{
    struct my_data *data;

    printk(KERN_INFO "RCU Demo: Initializing\n");

    // 分配初始数据
    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->value = 42;
    rcu_assign_pointer(global_data, data);

    // 创建两个读者线程
    reader1 = kthread_run(reader_thread, &reader1_id, "rcu_reader1");
    if (IS_ERR(reader1))
    {
        printk(KERN_ERR "Failed to create reader1 thread\n");
        kfree(data);
        return PTR_ERR(reader1);
    }

    reader2 = kthread_run(reader_thread, &reader2_id, "rcu_reader2");
    if (IS_ERR(reader2))
    {
        printk(KERN_ERR "Failed to create reader2 thread\n");
        kthread_stop(reader1);
        kfree(data);
        return PTR_ERR(reader2);
    }

    // 创建写者线程
    writer = kthread_run(writer_thread, NULL, "rcu_writer");
    if (IS_ERR(writer))
    {
        printk(KERN_ERR "Failed to create writer thread\n");
        kthread_stop(reader1);
        kthread_stop(reader2);
        kfree(data);
        return PTR_ERR(writer);
    }

    return 0;
}

static void __exit rcu_demo_exit(void)
{
    struct my_data *data;

    printk(KERN_INFO "RCU Demo: Exiting\n");

    // 停止线程
    running = 0;

    if (reader1)
        kthread_stop(reader1);
    if (reader2)
        kthread_stop(reader2);
    if (writer)
        kthread_stop(writer);

    // 清理数据
    data = rcu_dereference_protected(global_data, 1);
    rcu_assign_pointer(global_data, NULL);

    synchronize_rcu();
    kfree(data);
}

module_init(rcu_demo_init);
module_exit(rcu_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Multi-thread RCU Demo for Linux 5.10");