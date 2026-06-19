#include <linux/kernel.h>

#include "ops/op_fetch.h"

__u64 op_fetch(__u8 reg, __u8 off, __u64* bregs)
{
    if (reg >= BREG_NREGS || off >= BREG_REG_QWORDS) {
        return 0;
    }

    return smp_load_acquire(&bregs[reg * BREG_REG_QWORDS + off]);
}
