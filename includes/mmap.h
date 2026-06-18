#ifndef MMAP_H
#define MMAP_H

#include <linux/mm.h>

#define LABRCTL_DEVNAME "labrctl"

extern void* bufferpage;

int ummap(struct file* filp, struct vm_area_struct* vma);

#endif // MMAP_H
