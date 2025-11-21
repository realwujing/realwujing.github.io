#include <linux/module.h>
#include <linux/kernel.h>

static int my_function(void) {
    printk(KERN_INFO "Hello from module a\n");
    return 0;
}
EXPORT_SYMBOL(my_function);

static int __init a_init(void) {
    printk(KERN_INFO "Module a loaded\n");
    return 0;
}

static void __exit a_exit(void) {
    printk(KERN_INFO "Module a unloaded\n");
}

module_init(a_init);
module_exit(a_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("Module a");
