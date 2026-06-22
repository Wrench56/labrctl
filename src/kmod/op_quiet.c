#include "ops/op_quiet.h"

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/topology.h>
#include <linux/workqueue.h>

static int kfile_write(const char* path, const char* data, size_t len)
{
    loff_t off = 0;
    struct file* fp = filp_open(path, O_WRONLY, 0);
    if (IS_ERR(fp)) {
        return PTR_ERR(fp);
    }

    ssize_t n = kernel_write(fp, data, len, &off);
    filp_close(fp, NULL);

    if (n == len) {
        return 0;
    }

    return n;
}

static int kfile_read(const char* path, char* buf, size_t len)
{
    loff_t off = 0;
    struct file* fp = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        return PTR_ERR(fp);
    }

    ssize_t n = kernel_read(fp, buf, len - 1, &off);
    filp_close(fp, NULL);

    return n;
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

    for_each_irq_nr(irq)
    {
        irq_set_affinity(irq, hk_mask);
    }
}

static char set_numa(char state)
{
    char old[2] = { '?', 0 }, neu[2] = { state, 0 };
    kfile_read("/proc/sys/kernel/numa_balancing", old, sizeof(old));
    kfile_write("/proc/sys/kernel/numa_balancing", neu, 1);
    return old[0];
}

static int set_wq_cpumask(
    const struct cpumask* mask,
    char* save,
    size_t save_len
)
{
    char buf[16];
    if (save) {
        kfile_read("/sys/devices/virtual/workqueue/cpumask", save, save_len);
    }

    int len = scnprintf(buf, sizeof(buf), "%*pb", cpumask_pr_args(mask));
    return kfile_write("/sys/devices/virtual/workqueue/cpumask", buf, len);
}

static void push_tasks_away(
    const struct cpumask* hk_mask,
    struct cpumask* tmp_mask,
    pid_t skip_pid
)
{
    struct task_struct* g;
    struct task_struct* p;

    rcu_read_lock();
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
        rcu_read_unlock();

        set_cpus_allowed_ptr(p, tmp_mask);

        rcu_read_lock();
        put_task_struct(p);
    }

    rcu_read_unlock();
}

void op_quiet_set(struct labrctl_ctl* ctl, __u8* bufferpage)
{
    struct labrctl_quiet_save* save = (void*) &bufferpage[QSAVE_OFF];

    save->pinned_state = 0;
    save->numa_state = set_numa('0');

    __u8 ecpu = ctl->arg[0];
    if (!cpu_online(ecpu)) {
        pr_err("Experiment CPU is offline\n");
        return;
    }

    /* Turn off SMT sibling(s) */
    const struct cpumask* scpu_mask = topology_sibling_cpumask(ecpu);
    __u32 sib;

    save->scpu = 0;
    for_each_cpu(sib, scpu_mask)
    {
        if (cpu_online(sib) && sib != ecpu) {
            remove_cpu(sib);

            /*
             * If you have more than 2^16 cores, this is UB...
             * Also, technically you can only have 1 SMT sibling,
             * if you have some IBM POWER architecture, I can't help
             */
            save->scpu = sib;
        }
    }

    /* Move experiment task to experiment CPU */
    __u64 tmp;
    __builtin_memcpy(&tmp, ctl->data, sizeof(tmp));
    int user_pid = (int) tmp;

    struct pid* pid = find_get_pid(user_pid);
    if (!pid) {
        pr_err("Task PID not found\n");
        return;
    }
    struct task_struct* task = get_pid_task(pid, PIDTYPE_PID);
    put_pid(pid);
    if (!task) {
        pr_err("Task struct could not be retrieved\n");
        return;
    }

    set_cpus_allowed_ptr(task, cpumask_of(ecpu));
    put_task_struct(task);

    /* NR_CPUS at 8192 is absolutely insane... 
     * cpumask allocated on the stack overflows the
     * kernels 1024 byte stack frame limit. You
     * have to allocate here. Who has 8192 CPUs?!?!
     */
    cpumask_var_t hk_mask;
    if (!alloc_cpumask_var(&hk_mask, GFP_KERNEL)) {
        pr_err("Could not allocate hk_mask\n");
        return;
    }

    cpumask_var_t tmp_mask;
    if (!alloc_cpumask_var(&tmp_mask, GFP_KERNEL)) {
        free_cpumask_var(hk_mask);
        pr_err("Could not allocate tmp_mask\n");
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

    /* Disable CPU Boost mode */
    if (kfile_read(
            "/sys/devices/system/cpu/cpufreq/boost",
            &save->boost,
            sizeof(save->boost)
        ) > 0) {
        kfile_write("/sys/devices/system/cpu/cpufreq/boost", "0", 1);
        save->pinned_state |= QF_BOOST;
    }

    /* Disable C-states */
    cpu_latency_qos_add_request(&save->pmqreq, 0);
    save->pinned_state |= QF_PM;

    /* Pin experiment CPU's frequency to MAX */
    struct cpufreq_policy* policy = cpufreq_cpu_get(ecpu);
    if (!policy) {
        pr_err("Could not retrieve CPU frequency policy\n");
        return;
    }

    freq_qos_add_request(
        &policy->constraints,
        &save->freqqreq,
        FREQ_QOS_MIN,
        policy->cpuinfo.max_freq
    );
    cpufreq_cpu_put(policy);
    save->pinned_state |= QF_FREQ;
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
        add_cpu(scpu);
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

    /* Restore allowance for any C-states */
    if (save->pinned_state & QF_PM) {
        cpu_latency_qos_remove_request(&save->pmqreq);
    }

    /* Restore CPU frequency QOS */
    if (save->pinned_state & QF_FREQ) {
        freq_qos_remove_request(&save->freqqreq);
    }

    /* Restore CPU Boost mode */
    if (save->pinned_state & QF_BOOST) {
        kfile_write(
            "/sys/devices/system/cpu/cpufreq/boost",
            &save->boost,
            sizeof(save->boost)
        );
    }
}
