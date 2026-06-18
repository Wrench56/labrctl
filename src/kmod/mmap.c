#include "mmap.h"

int ummap(struct file* filp, struct vm_area_struct* vma)
{
    (void) filp;

    unsigned long length = vma->vm_end - vma->vm_start;
    if (length != PAGE_SIZE) {
        return -EINVAL;
    }

    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
    return vm_insert_page(vma, vma->vm_start, virt_to_page(bufferpage));
}
