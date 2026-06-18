#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "ops/op_kill.h"

void op_kill(struct labrctl_ctl* ctl, __u64* bufferpage)
{
    (void) bufferpage;
    __u64 tmp;

    __u8 signal = ctl->arg[0];
    if (!valid_signal(signal)) {
        pr_err("Invalid signal %u\n", signal);
        return;
    }

    __builtin_memcpy(&tmp, ctl->data, sizeof(tmp));
    int user_pid = (int) tmp;

    struct kernel_siginfo info;
    __builtin_memset(&info, 0, sizeof(struct kernel_siginfo));
    info.si_signo = signal;
    info.si_code = SI_QUEUE;
    info.si_int = 0;

    rcu_read_lock();
    struct pid* pid_struct = find_vpid(user_pid);
    if (!pid_struct) {
        pr_err("PID %d not found\n", user_pid);
        rcu_read_unlock();
        return;
    }

    struct task_struct* task = pid_task(pid_struct, PIDTYPE_PID);
    if (task) {
        if (send_sig_info(signal, &info, task) < 0) {
            pr_err("Error sending signal %d\n", signal);
        }
    } else {
        pr_err("Failed to map PID to task struct\n");
    }

    rcu_read_unlock();
}
