#include <linux/bpf.h>
#include <linux/btf.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "labrctl_ctl.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Mark Devenyi");
MODULE_DESCRIPTION("labrctl kernel module");
MODULE_VERSION("0.1");

static struct labrctl_ctl* ctl = { 0 };

__bpf_kfunc_start_defs();

__bpf_kfunc int bpf_labrctl_submit(void* data, size_t data__sz)
{
    const __u8* payload = data;
    if (data__sz < 2) {
        return -EINVAL;
    }

    /* XDP passes raw pointers to packets. Copy payload or crash! */
    WRITE_ONCE(ctl->ver, payload[1]);
    WRITE_ONCE(ctl->op, payload[2]);
    WRITE_ONCE(ctl->arg, payload[3]);

    smp_store_release(&ctl->epoch, READ_ONCE(ctl->epoch) + 1);

    return 0;
}

__bpf_kfunc_end_defs();

BTF_KFUNCS_START(labrctl_kfunc_ids);
BTF_ID_FLAGS(func, bpf_labrctl_submit);
BTF_KFUNCS_END(labrctl_kfunc_ids);

static const struct btf_kfunc_id_set labrctl_kfunc_set = {
    .owner = THIS_MODULE,
    .set = &labrctl_kfunc_ids,
};

static int __init labrctl_init(void)
{
    ctl = (void*) get_zeroed_page(GFP_KERNEL);
    if (!ctl) {
        return -ENOMEM;
    }

    int ret = register_btf_kfunc_id_set(BPF_PROG_TYPE_XDP, &labrctl_kfunc_set);
    if (ret) {
        free_page((unsigned long) ctl);
        pr_err("labrctl: Module failed to load while loading XDP\n");
        return ret;
    }

    pr_info("labrctl: Module loaded successfully\n");
    return 0;
}

static void __exit labrctl_exit(void)
{
    if (ctl) {
        free_page((unsigned long) ctl);
    }

    pr_info("labrctl: Module unloaded successfully\n");
}

module_init(labrctl_init);
module_exit(labrctl_exit);
