#ifndef LABRCTL_AGENT_H
#define LABRCTL_AGENT_H

#include "labrctl_ctl.h"

#define LIBLABRCTL_MAJOR 0
#define LIBLABRCTL_MINOR 1

#define LIBLABRCTL_STR1(x) #x
#define LIBLABRCTL_STR(x) LIBLABRCTL_STR1(x)
#define LIBLABRCTL_VERSION                                                     \
    LIBLABRCTL_STR(LIBLABRCTL_MAJOR) "." LIBLABRCTL_STR(LIBLABRCTL_MINOR)

const char* labrctl_agent_version(void);
struct labrctl_ctl* loadctl(void);
_Bool labrctl_poll(struct labrctl_ctl* ctl, uint64_t* epoch);

#endif // LABRCTL_AGENT_H
