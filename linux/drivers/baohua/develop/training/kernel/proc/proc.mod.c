#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x5ac9bae1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc45fef24, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0xbfd390f0, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0xcd03c6f3, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0xb33efd36, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0xb5fe7e7b, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x30e44994, __VMLINUX_SYMBOL_STR(proc_mkdir) },
	{ 0x20000329, __VMLINUX_SYMBOL_STR(simple_strtoul) },
	{ 0x6042549c, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0x4ea17c08, __VMLINUX_SYMBOL_STR(single_open) },
	{ 0xeee43f50, __VMLINUX_SYMBOL_STR(PDE_DATA) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "1CB1B0D439001E11EA5D6E1");
