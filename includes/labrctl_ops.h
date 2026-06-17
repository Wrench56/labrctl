#ifndef LABRCTL_OPS_H
#define LABRCTL_OPS_H

enum labrctl_op {
    LABRCTL_OP_NOP = 0,
    LABRCTL_OP_ACK = 1,
    LABRCTL_OP_SPAWN = 2,
    LABRCTL_OP_KILL = 3,
};

#endif // LABRCTL_OPS_H
