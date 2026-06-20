#ifndef LABRCTL_CTL_H
#define LABRCTL_CTL_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#define LABRCTL_MAJOR 0
#define LABRCTL_MINOR 1
#define LABRCTL_MAGIC 0x4C
#define LABRCTL_VERSION 0x00
#define LABRCTL_PORT 19552

#define LABRCTL_STR1(x) #x
#define LABRCTL_STR(x) LABRCTL_STR1(x)
#define LABRCTL_VERSION_STR                                                    \
    LABRCTL_STR(LABRCTL_MAJOR) "." LABRCTL_STR(LABRCTL_MINOR)

enum labrctl_op {
    LABRCTL_OP_NOP = 0,
    LABRCTL_OP_ACK = 1,
    LABRCTL_OP_RESEQ = 2,
    LABRCTL_OP_STORE = 3,
    LABRCTL_OP_FETCH = 4,
    LABRCTL_OP_SPAWN = 5,
    LABRCTL_OP_KILL = 6,
    LABRCTL_OP_QUIET_SET = 7,
    LABRCTL_OP_QUIET_RESTORE = 8,
    LABRCTL_OP_GPUSIG0 = 128,
    LABRCTL_OP_GPUSIG1 = 129,
    LABRCTL_OP_GPUSIG2 = 130,
    LABRCTL_OP_GPUSIG3 = 131,
};

struct labrctl_ctl_client {
    int fd;
    uint8_t seq;
    uint8_t retries;
    struct timeval timeout;
};

struct labrctl_ctl_packet {
    uint16_t hdr;
    uint8_t op;
    uint8_t arg[2];
    uint8_t seq;
    uint8_t data[8];
    uint8_t rsvd[4];
} __attribute__((packed));

int labrctl_ctl_open(
    struct labrctl_ctl_client* c,
    const char* server,
    uint16_t port,
    unsigned timeout_ms,
    uint8_t retries
);

void labrctl_ctl_close(struct labrctl_ctl_client* c);

int labrctl_ctl_command(
    struct labrctl_ctl_client* c,
    uint8_t op,
    const uint8_t arg[2],
    const uint8_t data[8]
);

int labrctl_ctl_resync(struct labrctl_ctl_client* c);

int labrctl_ctl_fetch(
    struct labrctl_ctl_client* c,
    uint8_t reg,
    uint8_t off,
    uint64_t* out
);

const char* labrctl_ctl_version(void);

static inline int labrctl_ctl_nop(struct labrctl_ctl_client* c)
{
    return labrctl_ctl_command(c, LABRCTL_OP_NOP, NULL, NULL);
}

static inline int labrctl_ctl_store(
    struct labrctl_ctl_client* c,
    uint8_t reg,
    uint8_t off,
    const uint8_t data[8]
)
{
    const uint8_t arg[2] = { reg, off };
    return labrctl_ctl_command(c, LABRCTL_OP_STORE, arg, data);
}

static inline int labrctl_ctl_spawn(
    struct labrctl_ctl_client* c,
    const uint8_t arg[2]
)
{
    return labrctl_ctl_command(c, LABRCTL_OP_SPAWN, arg, NULL);
}

static inline int labrctl_ctl_kill(
    struct labrctl_ctl_client* c,
    int sig,
    pid_t pid
)
{
    uint8_t arg[2] = { (uint8_t) sig, 0 };
    uint8_t data[8] = { 0 };

    uint64_t tmp = (uint64_t) pid;
    memcpy(data, &tmp, sizeof(tmp));

    return labrctl_ctl_command(c, LABRCTL_OP_KILL, arg, data);
}

#endif // LABRCTL_CTL_H
