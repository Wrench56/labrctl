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

static void* structpage = NULL;
static struct labrctl_ctl* ctl = NULL;
static struct labrctl_ctl* thrd_ctl = NULL;
static struct task_struct* worker_thrd = NULL;

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

static int worker_fn(void* payload)
{
    struct labrctl_ctl* ctl = payload;
    while (!kthread_should_stop()) {
        // TODO: Add K-ops handling here
        switch (ctl->op) {
            default:
                pr_err("labrctl: Invalid opcode in worker kthread\n");
                return 1;
        }
    }

    pr_info("labrctl: Shutting down worker thread\n");
    return 0;
}

static int __init labrctl_init(void)
{
    structpage = (void*) get_zeroed_page(GFP_KERNEL);
    if (!structpage) {
        return -ENOMEM;
    }

    ctl = (struct labrctl_ctl*) structpage;
    thrd_ctl = ctl + 1;
    worker_thrd = (void*) thrd_ctl + 1;

    int ret = register_btf_kfunc_id_set(BPF_PROG_TYPE_XDP, &labrctl_kfunc_set);
    if (ret) {
        free_page((unsigned long) ctl);
        pr_err("labrctl: Module failed to load while loading XDP\n");
        return ret;
    }

    /* Dumb Linux legacy convention of calling threads tasks... Why?! */
    worker_thrd = kthread_run(worker_fn, &thrd_ctl, "labrctl_worker");
    if (IS_ERR(worker_thrd)) {
        pr_err("labrctl: Failed to create worker\n");
        return PTR_ERR(worker_thrd);
    }

    pr_info("labrctl: Module loaded successfully\n");
    return 0;
}

static void __exit labrctl_exit(void)
{
    kthread_stop(worker_thrd);

    if (structpage) {
        free_page((unsigned long) structpage);
    }

    pr_info("labrctl: Module unloaded successfully\n");
}

module_init(labrctl_init);
module_exit(labrctl_exit);
