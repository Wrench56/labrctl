#ifndef LABRCTL_CTL_H
#define LABRCTL_CTL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#define LIBLABRCTL_MAJOR 0
#define LIBLABRCTL_MINOR 1

#define LIBLABRCTL_STR1(x) #x
#define LIBLABRCTL_STR(x) LIBLABRCTL_STR1(x)
#define LIBLABRCTL_VERSION                                                     \
    LIBLABRCTL_STR(LIBLABRCTL_MAJOR) "." LIBLABRCTL_STR(LIBLABRCTL_MINOR)

enum labrctl_op {
    LABRCTL_OP_NOP = 0,
    LABRCTL_OP_ACK = 1,
    LABRCTL_OP_STORE = 2,
    LABRCTL_OP_SPAWN = 3,
    LABRCTL_OP_KILL = 4,
    LABRCTL_OP_RESEQ = 5,
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

const char* labrctl_ctl_version(void);

static inline int labrctl_ctl_nop(struct labrctl_ctl_client* c)
{
    return labrctl_ctl_command(c, LABRCTL_OP_NOP, NULL, NULL);
}

static inline int labrctl_ctl_store(
    struct labrctl_ctl_client* c,
    const uint8_t data[8]
)
{
    return labrctl_ctl_command(c, LABRCTL_OP_STORE, NULL, data);
}

static inline int labrctl_ctl_spawn(
    struct labrctl_ctl_client* c,
    const uint8_t arg[2]
)
{
    return labrctl_ctl_command(c, LABRCTL_OP_SPAWN, arg, NULL);
}

static inline int labrctl_ctl_kill(struct labrctl_ctl_client* c)
{
    return labrctl_ctl_command(c, LABRCTL_OP_KILL, NULL, NULL);
}

#endif // LABRCTL_CTL_H
