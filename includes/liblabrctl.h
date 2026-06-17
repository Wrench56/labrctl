#ifndef LIBLABRCTL_H
#define LIBLABRCTL_H

#define LIBLABRCTL_MAJOR 0
#define LIBLABRCTL_MINOR 1

#define LIBLABRCTL_STR1(x) #x
#define LIBLABRCTL_STR(x) LIBLABRCTL_STR1(x)

#define LIBLABRCTL_VERSION                                                     \
    LIBLABRCTL_STR(LIBLABRCTL_MAJOR) "." LIBLABRCTL_STR(LIBLABRCTL_MINOR)

const char* labrctl_version(void);

#endif // LIBLABRCTL_H
