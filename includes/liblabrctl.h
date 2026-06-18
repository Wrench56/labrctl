#ifndef LIBLABRCTL_H
#define LIBLABRCTL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#include "labrctl_ctl.h"
#include "labrctl_ops.h"

#define LIBLABRCTL_MAJOR 0
#define LIBLABRCTL_MINOR 1

#define LIBLABRCTL_STR1(x) #x
#define LIBLABRCTL_STR(x) LIBLABRCTL_STR1(x)
#define LIBLABRCTL_VERSION                                                     \
    LIBLABRCTL_STR(LIBLABRCTL_MAJOR) "." LIBLABRCTL_STR(LIBLABRCTL_MINOR)

struct labrctl_client {
    int fd;
    uint8_t seq;
    uint8_t retries;
    struct timeval timeout;
};

int labrctl_open(
    struct labrctl_client* c,
    const char* server,
    uint16_t port,
    unsigned timeout_ms,
    uint8_t retries
);

void labrctl_close(struct labrctl_client* c);

int labrctl_command(
    struct labrctl_client* c,
    uint8_t op,
    const uint8_t arg[2],
    const uint8_t data[8]
);

int labrctl_resync(struct labrctl_client* c);

const char* labrctl_version(void);

static inline int labrctl_nop(struct labrctl_client* c)
{
    return labrctl_command(c, LABRCTL_OP_NOP, NULL, NULL);
}

static inline int labrctl_store(struct labrctl_client* c, const uint8_t data[8])
{
    return labrctl_command(c, LABRCTL_OP_STORE, NULL, data);
}

static inline int labrctl_spawn(struct labrctl_client* c, const uint8_t arg[2])
{
    return labrctl_command(c, LABRCTL_OP_SPAWN, arg, NULL);
}

static inline int labrctl_kill(struct labrctl_client* c)
{
    return labrctl_command(c, LABRCTL_OP_KILL, NULL, NULL);
}

#endif // LIBLABRCTL_H
