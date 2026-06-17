#ifndef LABRCTL_CTL_H
#define LABRCTL_CTL_H

#include "labrctl_ops.h"

#define LABRCTL_PORT 19552

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint64_t __u64;
#endif

#define LABRCTL_MAGIC 0x4C
#define LABRCTL_VERSION 0x00

/* Should fit into a single cache-line counting ETH + IP + UDP header */
struct labrctl_packet {
    /* Has to be LABRCTL_MAGIC & LABRCTL_VERSION */
    __u16 hdr;
    __u8 op;
    __u8 arg[2];
    __u8 seq;
    __u8 data[8];
    __u8 rsvd[4];
} __attribute__((packed));

struct labrctl_ctl {
    __u64 epoch;
    __u8 ver;
    __u8 op;
    __u8 arg[2];
    __u8 data[8];
    __u8 rsvd[44];
} __attribute__((packed));

#define CTL_SZ sizeof(struct labrctl_ctl)
#define PACKET_SZ sizeof(struct labrctl_packet)

#define DATA_OFFS 6

#endif // LABRCTL_CTL_H
