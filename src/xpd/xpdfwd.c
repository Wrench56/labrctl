#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("xdp")
int xdp_fwd(struct xdp_md* ctx)
{
    (void) ctx;
    return XDP_PASS;
}

char _license[] SEC("license") = "MIT";
