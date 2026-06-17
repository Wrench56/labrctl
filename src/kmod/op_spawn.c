#include <linux/kmod.h>
#include <linux/pid.h>

#include "ops/op_spawn.h"
#include "ops/op_store.h"

static void split(char* p, char** v, int max)
{
    int n = 0;
    char* end = p + BREG_REG_BYTES;

    while (p < end && n < max - 1) {
        while (p < end && !*p) {
            p++;
        }
        if (p >= end) {
            break;
        }

        v[n++] = p;

        while (p < end && *p) {
            p++;
        }
    }

    v[n] = NULL;
}

static int spawn_init(struct subprocess_info* info, struct cred* new)
{
    __u64* pid_out = info->data;

    WRITE_ONCE(*pid_out, (__u64) task_pid_nr(current));
    return 0;
}

void op_spawn(struct labrctl_ctl* ctl, __u64* bufferpage)
{
    (void) ctl;

    char* base = (char*) bufferpage;
    char* argv[MAX_ARGV] = { 0 };
    char* envp[MAX_ENVP] = { 0 };

    base[2 * BREG_REG_BYTES - 1] = 0;
    base[3 * BREG_REG_BYTES - 1] = 0;

    split(base + BREG_REG_BYTES, argv, MAX_ARGV);
    split(base + 2 * BREG_REG_BYTES, envp, MAX_ENVP);
    struct subprocess_info* sub_info = call_usermodehelper_setup(
        argv[0],
        argv,
        envp,
        GFP_KERNEL,
        spawn_init,
        NULL,
        base
    );

    if (!sub_info) {
        WRITE_ONCE(((__u64*) base)[0], 0);
        return;
    }

    int ret = call_usermodehelper_exec(sub_info, UMH_WAIT_EXEC);
    if (ret) {
        WRITE_ONCE(((__u64*) base)[0], 0);
        return;
    }
}
