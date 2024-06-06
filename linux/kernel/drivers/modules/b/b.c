#include <linux/module.h>
#include <linux/kernel.h>

extern int my_function(void);

static int __init b_init(void) {
    printk(KERN_INFO "Module b loaded\n");
    my_function();
    return 0;
}

static void __exit b_exit(void) {
    printk(KERN_INFO "Module b unloaded\n");
}

module_init(b_init);
module_exit(b_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("Module b");
