#include "labrctl_agent.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/syscall.h>

const char* labrctl_agent_version(void)
{
    return LIBLABRCTL_VERSION;
}

struct labrctl_ctl* loadctl(void)
{
#if defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    long ret = 0;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(SYS_open), "D"("/dev/labrctl"), "S"(O_RDWR), "d"(0)
                     : "rcx", "r11", "memory");

    int fd = ret;
    if (fd < 0) {
        return NULL;
    }

    register long r10 __asm__("r10") = MAP_SHARED;
    register long r8 __asm__("r8") = fd;
    register long r9 __asm__("r9") = 0;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(SYS_mmap),
                       "D"(NULL),
                       "S"(sizeof(struct labrctl_ctl)),
                       "d"(PROT_READ | PROT_WRITE),
                       "r"(r10),
                       "r"(r8),
                       "r"(r9)
                     : "rcx", "r11", "memory");

    struct labrctl_ctl* ctl = (void*) ret;
    if (ret < 0) {
        ctl = NULL;
    }

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(SYS_close), "D"(fd)
                     : "rcx", "r11", "memory");

    return ctl;

#else
#error "I do not know AARCH, leave me alone"
#endif
}

_Bool labrctl_poll(struct labrctl_ctl* ctl, uint64_t* epoch)
{
    uint64_t cur = ctl->epoch;
    if (cur == *epoch) {
        return 0;
    }

    *epoch = cur;
    return 1;
}
