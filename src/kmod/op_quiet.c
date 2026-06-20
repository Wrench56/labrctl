#include "ops/op_quiet.h"

#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/topology.h>
#include <linux/workqueue.h>

static char set_numa(char state)
{
    loff_t loff = 0;
    struct file* fp = filp_open(
        "/proc/sys/kernel/numa_balancing",
        O_RDWR,
        0644
    );

    if (IS_ERR(fp)) {
        return '?';
    }

    char ret[1] = { '?' };
    kernel_read(fp, ret, sizeof(ret), &loff);

    loff = 0;
    char buf[2] = "0";
    buf[0] = state;
    kernel_write(fp, buf, sizeof(buf) - 1, &loff);

    filp_close(fp, NULL);
    return ret[0];
}

static void set_wq_cpumask(
    const struct cpumask* mask,
    char* save_buf,
    size_t save_len
)
{
    loff_t off = 0;
    char buf[16];
    struct file* fp = filp_open(
        "/sys/devices/virtual/workqueue/cpumask",
        O_RDWR,
        0644
    );

    if (IS_ERR(fp)) {
        return;
    }

    if (save_buf) {
        int n = kernel_read(fp, save_buf, save_len - 1, &off);
        save_buf[n > 0 ? n : 0] = '\0';
        off = 0;
    }

    int len = scnprintf(buf, sizeof(buf), "%*pb", cpumask_pr_args(mask));
    kernel_write(fp, buf, len, &off);
    filp_close(fp, NULL);
}

static int str_to_cpumask(const char* buf, struct cpumask* mask)
{
    char tmp[16];
    strscpy(tmp, buf, sizeof(tmp));
    strim(tmp);

    return cpumask_parse(tmp, mask);
}

static void set_irq_affinity(const struct cpumask* hk_mask)
{
    unsigned int irq;
    struct irq_desc* desc;

    for_each_irq_desc(irq, desc)
    {
        if (!desc) {
            continue;
        }

        if (!irq_can_set_affinity(irq)) {
            continue;
        }

        irq_set_affinity(irq, hk_mask);
    }
}

static void push_tasks_away(
    const struct cpumask* hk_mask,
    struct cpumask* tmp_mask,
    pid_t skip_pid
)
{
    struct task_struct* g;
    struct task_struct* p;

    read_lock(&tasklist_lock);
    for_each_process_thread(g, p)
    {
        if (p == current) {
            continue;
        }

        if (p->flags & PF_KTHREAD) {
            continue;
        }

        if (p->pid <= 1) {
            continue;
        }

        if (p->tgid == skip_pid) {
            continue;
        }

        cpumask_and(tmp_mask, p->cpus_ptr, hk_mask);
        if (cpumask_empty(tmp_mask)) {
            continue;
        }

        get_task_struct(p);
        read_unlock(&tasklist_lock);

        set_cpus_allowed_ptr(p, tmp_mask);
        put_task_struct(p);

        read_lock(&tasklist_lock);
    }

    read_unlock(&tasklist_lock);
}

void op_quiet_set(struct labrctl_ctl* ctl, __u8* bufferpage)
{
    struct labrctl_quiet_save* save = (void*) &bufferpage[QSAVE_OFF];

    save->numa_state = set_numa('0');

    __u8 ecpu = ctl->arg[0];
    if (!cpu_online(ecpu)) {
        return;
    }

    /* Turn off SMT sibling(s) */
    const struct cpumask* scpu_mask = topology_sibling_cpumask(ecpu);
    __u32 sib;

    save->scpu = 0;
    for_each_cpu(sib, scpu_mask)
    {
        if (cpu_online(sib) && sib != ecpu) {
            struct device* cpud = get_cpu_device(sib);
            device_offline(cpud);

            /*
             * If you have more than 2^16 cores, this is UB...
             * Also, technically you can only have 1 SMT sibling,
             * if you have some IBM POWER architecture, I can't help
             */
            save->scpu = sib;
        }
    }

    __u64 tmp;
    __builtin_memcpy(&tmp, ctl->data, sizeof(tmp));
    int user_pid = (int) tmp;

    rcu_read_lock();
    struct task_struct* task = find_task_by_vpid(user_pid);
    if (!task) {
        rcu_read_unlock();
        return;
    }
    get_task_struct(task);
    rcu_read_unlock();

    set_cpus_allowed_ptr(task, cpumask_of(ecpu));
    put_task_struct(task);

    /* NR_CPUS at 8192 is absolutely insane... 
     * cpumask allocated on the stack overflows the
     * kernels 1024 byte stack frame limit. You
     * have to allocate here. Who has 8192 CPUs?!?!
     */
    cpumask_var_t hk_mask;
    if (!alloc_cpumask_var(&hk_mask, GFP_KERNEL)) {
        return;
    }

    cpumask_var_t tmp_mask;
    if (!alloc_cpumask_var(&tmp_mask, GFP_KERNEL)) {
        free_cpumask_var(hk_mask);
        return;
    }

    /* Push everything to CPUs 0-7 */
    cpumask_clear(hk_mask);
    cpumask_set_cpu(0, hk_mask);
    cpumask_set_cpu(1, hk_mask);
    cpumask_set_cpu(2, hk_mask);
    cpumask_set_cpu(3, hk_mask);
    cpumask_set_cpu(4, hk_mask);
    cpumask_set_cpu(5, hk_mask);
    cpumask_set_cpu(6, hk_mask);
    cpumask_set_cpu(7, hk_mask);

    /* Ugly workaround since workqueue_set_unbound_cpumask() is not exported... */
    set_wq_cpumask(hk_mask, save->wqcpumask, sizeof(save->wqcpumask));

    set_irq_affinity(hk_mask);
    push_tasks_away(hk_mask, tmp_mask, user_pid);

    free_cpumask_var(hk_mask);
    free_cpumask_var(tmp_mask);
}

void op_quiet_restore(struct labrctl_ctl* ctl, __u8* bufferpage)
{
    struct labrctl_quiet_save* save = (void*) &bufferpage[QSAVE_OFF];

    char saved_numa = save->numa_state;
    if (saved_numa == '?') {
        saved_numa = '0';
    }
    set_numa(saved_numa);

    /* Restore sibling core */
    __u16 scpu = save->scpu;
    if (scpu != 0 && cpu_is_offline(scpu)) {
        struct device* cpud = get_cpu_device(scpu);
        device_online(cpud);
    }

    /* Restore workqueue CPU mask */
    cpumask_var_t wq_mask;
    if (!alloc_cpumask_var(&wq_mask, GFP_KERNEL)) {
        return;
    }

    if (str_to_cpumask(save->wqcpumask, wq_mask) == 0) {
        set_wq_cpumask(wq_mask, NULL, 0);
    }
    free_cpumask_var(wq_mask);
}
