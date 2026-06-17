#include <linux/kmod.h>

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

void op_spawn(struct labrctl_ctl* ctl, __u64* bufferpage)
{
    char* base = (char*) bufferpage;
    char* argv[MAX_ARGV] = { 0 };
    char* envp[MAX_ENVP] = { 0 };

    base[BREG_REG_BYTES - 1] = 0;
    base[2 * BREG_REG_BYTES - 1] = 0;

    split(base, argv, MAX_ARGV);
    split(base + BREG_REG_BYTES, envp, MAX_ENVP);

    call_usermodehelper(argv[0], argv, envp, UMH_NO_WAIT);
}
