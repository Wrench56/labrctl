#ifndef OP_QUIET_H
#define OP_QUIET_H

#include "labrctl_ctl.h"

#define QSAVE_OFF (2 * sizeof(struct labrctl_ctl))

struct labrctl_quiet_save {
    char numa_state;
    __u16 scpu;
} __attribute__((packed));

void op_quiet_set(struct labrctl_ctl* ctl, __u8* bufferpage);
void op_quiet_restore(struct labrctl_ctl* ctl, __u8* bufferpage);

#endif // OP_QUIET_H
