#ifndef OP_SPAWN_H
#define OP_SPAWN_H

#include "labrctl_ctl.h"

#define MAX_ARGV 16
#define MAX_ENVP 16

void op_spawn(struct labrctl_ctl* ctl, __u64* bufferpage);

#endif // OP_SPAWN_H
