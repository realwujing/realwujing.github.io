#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>

// 初始化迭代器，返回第一个数据项
static void *my_seq_start(struct seq_file *m, loff_t *pos)
{
    if (*pos >= 10) // 如果位置超出范围，返回 NULL 表示结束
        return NULL;
    return pos; // 返回当前迭代器位置
}

// 获取下一个数据项
static void *my_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    (*pos)++;       // 增加位置
    if (*pos >= 10) // 如果位置超出范围，返回 NULL 表示结束
        return NULL;
    return pos; // 返回新的迭代器位置
}

// 停止迭代，释放资源（如果有）
static void my_seq_stop(struct seq_file *m, void *v)
{
    // 此处无需释放资源
}

// 将当前数据项写入缓冲区
static int my_seq_show(struct seq_file *m, void *v)
{
    loff_t *spos = (loff_t *)v;             // 获取当前迭代器位置
    seq_printf(m, "Number: %lld\n", *spos); // 输出当前数字
    return 0;
}

// 定义 seq_operations 结构体
static const struct seq_operations my_seq_ops = {
    .start = my_seq_start,
    .next = my_seq_next,
    .stop = my_seq_stop,
    .show = my_seq_show,
};

// 打开文件时调用，初始化 seq_file
static int my_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &my_seq_ops); // 绑定 seq_operations
}

// 定义 proc_ops 结构体
static const struct proc_ops my_proc_ops = {
    .proc_open = my_open,        // 打开文件
    .proc_read = seq_read,       // 读取文件
    .proc_lseek = seq_lseek,     // 调整文件偏移
    .proc_release = seq_release, // 关闭文件
};

#define PROC_FILENAME "my_seq_file"

// 模块加载时调用
static int __init my_seq_file_init(void)
{
    // 在 /proc 文件系统中创建文件
    proc_create(PROC_FILENAME, 0, NULL, &my_proc_ops);
    pr_info("/proc/%s created\n", PROC_FILENAME);
    return 0;
}

// 模块卸载时调用
static void __exit my_seq_file_exit(void)
{
    // 删除 /proc 文件
    remove_proc_entry(PROC_FILENAME, NULL);
    pr_info("/proc/%s removed\n", PROC_FILENAME);
}

module_init(my_seq_file_init);
module_exit(my_seq_file_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple seq_file demo");