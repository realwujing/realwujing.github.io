#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>

// 将当前数据项写入缓冲区
static int my_seq_show(struct seq_file *m, void *v)
{
    int i;
    for (i = 0; i < 10; i++) {
        seq_printf(m, "Number: %d\n", i); // 输出数字 0 到 9
    }
    return 0;
}

// 打开文件时调用，初始化 single_open
static int my_open(struct inode *inode, struct file *file)
{
    return single_open(file, my_seq_show, NULL); // 使用 single_open
}

// 定义 proc_ops 结构体
static const struct proc_ops my_proc_ops = {
    .proc_open = my_open,        // 打开文件
    .proc_read = seq_read,       // 读取文件
    .proc_lseek = seq_lseek,     // 调整文件偏移
    .proc_release = single_release, // 关闭文件
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
MODULE_DESCRIPTION("A simple single_open demo");