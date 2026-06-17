#ifndef OP_STORE_H
#define OP_STORE_H

#include "labrctl_ctl.h"

#define BREG_START_BYTES 512
#define BREG_REG_BYTES 256
#define BREG_REG_QWORDS (BREG_REG_BYTES / sizeof(__u64))
#define BREG_NREGS 8

void op_store(struct labrctl_ctl* ctl, __u64* bufferpage);

#endif // OP_STORE_H
