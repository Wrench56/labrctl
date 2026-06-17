#include <linux/bpf.h>
#include <linux/btf.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pagemap.h>

#include "labrctl_ctl.h"
#include "ops/ops.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Mark Devenyi");
MODULE_DESCRIPTION("labrctl kernel module");
MODULE_VERSION("0.1");

static void* bufferpage = NULL;
static struct labrctl_ctl* ctl = NULL;
static struct labrctl_ctl* thrd_ctl = NULL;

static struct task_struct* worker_thrd = { 0 };

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
    WRITE_ONCE(ctl->arg[0], payload[3]);
    WRITE_ONCE(ctl->arg[1], payload[4]);

    __builtin_memcpy(ctl->data, &payload[DATA_OFFS], sizeof(__u8[8]));
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
    __u64* bp = (__u64*) ((__u8*) bufferpage + BREG_START_BYTES);

    while (!kthread_should_stop()) {
        switch (ctl->op) {
            case LABRCTL_OP_STORE:
                op_store(ctl, bp);
                break;
            case LABRCTL_OP_SPAWN:
                op_spawn(ctl, bp);
                break;
            default:
                pr_err("labrctl: Invalid opcode in worker kthread\n");
                return 1;
        }
    }

    pr_info("labrctl: Shutting down worker thread\n");
    return 0;
}

static void free_bufferpage(void)
{
    if (!bufferpage) {
        return;
    }

    struct page* page_struct = virt_to_page(bufferpage);
    lock_page(page_struct);
    SetPageReclaim(page_struct);
    put_page(page_struct);
    unlock_page(page_struct);

    free_page((unsigned long) bufferpage);
}

static int __init labrctl_init(void)
{
    bufferpage = (void*) get_zeroed_page(GFP_KERNEL);
    if (!bufferpage) {
        return -ENOMEM;
    }

    struct page* page_struct = virt_to_page(bufferpage);
    lock_page(page_struct);
    get_page(page_struct);
    /* Undocumented. Please do what I think you do... */
    SetPagePinned(page_struct);
    unlock_page(page_struct);

    ctl = (struct labrctl_ctl*) bufferpage;
    thrd_ctl = ctl + 1;

    int ret = register_btf_kfunc_id_set(BPF_PROG_TYPE_XDP, &labrctl_kfunc_set);
    if (ret) {
        free_bufferpage();
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
    free_bufferpage();

    pr_info("labrctl: Module unloaded successfully\n");
}

module_init(labrctl_init);
module_exit(labrctl_exit);
