#include <linux/kernel.h>

#include "ops/op_store.h"

void op_store(struct labrctl_ctl* ctl, __u64* bregs)
{
    __u8 reg = READ_ONCE(ctl->arg[0]);
    __u8 off = READ_ONCE(ctl->arg[1]);
    if (reg >= BREG_NREGS || off >= BREG_REG_QWORDS) {
        return;
    }

    __u64 val;
    __builtin_memcpy(&val, ctl->data, sizeof(val));
    WRITE_ONCE(bregs[reg * BREG_REG_QWORDS + off], val);
}
