#include "ops/op_quiet.h"

#include <linux/cpu.h>
#include <linux/fs.h>
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

static void push_tasks_away(const struct cpumask* hk_mask, pid_t skip_pid)
{
    struct task_struct* g;
    struct task_struct* p;
    struct cpumask tmp;

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

        cpumask_and(&tmp, p->cpus_ptr, hk_mask);
        if (cpumask_empty(&tmp)) {
            continue;
        }

        get_task_struct(p);
        read_unlock(&tasklist_lock);

        set_cpus_allowed_ptr(p, &tmp);
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

    /* Push everything to CPUs 0-7 */
    struct cpumask hk_mask;

    cpumask_clear(&hk_mask);
    cpumask_set_cpu(0, &hk_mask);
    cpumask_set_cpu(1, &hk_mask);
    cpumask_set_cpu(2, &hk_mask);
    cpumask_set_cpu(3, &hk_mask);
    cpumask_set_cpu(4, &hk_mask);
    cpumask_set_cpu(5, &hk_mask);
    cpumask_set_cpu(6, &hk_mask);
    cpumask_set_cpu(7, &hk_mask);

    workqueue_unbound_housekeeping_update(&hk_mask);
    push_tasks_away(&hk_mask, user_pid);
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
}
