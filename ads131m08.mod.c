#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xc1514a3b, "free_irq" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xe08736b, "devm_kmalloc" },
	{ 0xa6875084, "proc_create" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x5b3a8d9f, "gpiod_set_value_cansleep" },
	{ 0xa4e5832c, "class_destroy" },
	{ 0x96848186, "scnprintf" },
	{ 0x9953258d, "gpiod_to_irq" },
	{ 0x37a0cba, "kfree" },
	{ 0xe67c066d, "seq_lseek" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xe2964344, "__wake_up" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x122c3a7e, "_printk" },
	{ 0x1d24c881, "___ratelimit" },
	{ 0x1000e51, "schedule" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x39f012c6, "cdev_add" },
	{ 0x8913bb9b, "spi_sync" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xfbe72bcf, "device_create" },
	{ 0x3fa498ad, "class_create" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xdf0adb77, "driver_unregister" },
	{ 0xdcb764ad, "memset" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x6dfbc1bf, "proc_remove" },
	{ 0x1f955396, "seq_read" },
	{ 0xeb991995, "spi_setup" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x521002df, "__spi_register_driver" },
	{ 0xd5c94a99, "devm_gpiod_get" },
	{ 0xb86bb5ac, "device_destroy" },
	{ 0x404ea81a, "seq_printf" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x91ab7f29, "single_release" },
	{ 0xa65c6def, "alt_cb_patch_nops" },
	{ 0x1c43a8f, "kmalloc_trace" },
	{ 0xc73bf268, "single_open" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xf9a482f9, "msleep" },
	{ 0xc4f0da12, "ktime_get_with_offset" },
	{ 0xe78e8907, "cdev_init" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x42b07433, "kmalloc_caches" },
	{ 0xa4afd25e, "cdev_del" },
	{ 0x67a35d9, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cti,ads131m08");
MODULE_ALIAS("of:N*T*Cti,ads131m08C*");
MODULE_ALIAS("spi:ads131m08");

MODULE_INFO(srcversion, "AAD9E2AFF3174FCAC358CCB");
