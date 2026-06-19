#ifndef OP_FETCH_H
#define OP_FETCH_H

#include "labrctl_ctl.h"

#ifndef BREG_START_BYTES
#define BREG_START_BYTES 512
#define BREG_REG_BYTES 256
#define BREG_REG_QWORDS (BREG_REG_BYTES / sizeof(__u64))
#define BREG_NREGS 8
#endif

__u64 op_fetch(__u8 reg, __u8 off, __u64* bufferpage);

#endif // OP_FETCH_H
