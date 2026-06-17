#ifndef LABRCTL_CTL_H
#define LABRCTL_CTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint8_t __u8;
typedef uint64_t __u64;
#endif

enum labrctl_op {
    LABRCTL_OP_NOP = 0,
    LABRCTL_OP_RUN = 1,
    LABRCTL_OP_KILL = 2,
};

struct labrctl_ctl {
    __u64 epoch;
    __u8 ver;
    __u8 op;
    __u8 arg;
    __u8 rsvd[53];
} __attribute__((packed)) ;

#define CTL_SZ sizeof(struct labrctl_packet)

#endif // LABRCTL_CTL_H
