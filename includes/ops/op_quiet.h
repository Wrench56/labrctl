#ifndef OP_QUIET_H
#define OP_QUIET_H

#include <linux/pm_qos.h>

#include "labrctl_ctl.h"

#define QSAVE_OFF (2 * sizeof(struct labrctl_ctl))

#define QF_FREQ (1 << 0)

struct labrctl_quiet_save {
    char numa_state;
    __u8 pinned_state;
    __u16 scpu;
    char wqcpumask[16];
    struct freq_qos_request freqqreq;
};

void op_quiet_set(struct labrctl_ctl* ctl, __u8* bufferpage);
void op_quiet_restore(struct labrctl_ctl* ctl, __u8* bufferpage);

#endif // OP_QUIET_H
