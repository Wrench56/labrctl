#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Mark Devenyi");
MODULE_DESCRIPTION("labrctl kernel module");
MODULE_VERSION("0.1");

static int __init labrctl_init(void)
{
    pr_info("labrctl: Module loaded successfully\n");
    return 0;
}

static void __exit labrctl_exit(void)
{
    pr_info("labrctl: Module unloaded successfully\n");
}

module_init(labrctl_init);
module_exit(labrctl_exit);
