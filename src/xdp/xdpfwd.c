#include <linux/types.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include "labrctl_ctl.h"

#define ETHHLEN (sizeof(struct ethhdr))
#define UDPHLEN (sizeof(struct udphdr))

extern __u64 bpf_labrctl_submit(void* data, __u64 data__sz) __ksym;

static __u64 seq_seen = 0;
static int xdp_handle_foreign = XDP_PASS;

static __always_inline int ack_tx(struct xdp_md* ctx, __u64 data)
{
    void* packet = (void*) (long) ctx->data;
    void* packet_end = (void*) (long) ctx->data_end;

    struct iphdr* iph;
#if TAILSCALE_IFINDEX != 0
    if (ctx->ingress_ifindex == TAILSCALE_IFINDEX) {
        iph = (struct iphdr*) packet;
        if ((void*) (iph + 1) > packet_end) {
            return XDP_DROP;
        }
    } else {
#endif
        struct ethhdr* eth = packet;
        if ((void*) (eth + 1) > packet_end) {
            return XDP_DROP;
        }

        iph = (struct iphdr*) (eth + 1);
        if ((void*) (iph + 1) > packet_end) {
            return XDP_DROP;
        }

        __u8 mac[ETH_ALEN];
        __builtin_memcpy(mac, eth->h_source, ETH_ALEN);
        __builtin_memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
        __builtin_memcpy(eth->h_dest, mac, ETH_ALEN);
#if TAILSCALE_IFINDEX != 0
    }
#endif

    struct udphdr* udp = (struct udphdr*) (iph + 1);
    if ((void*) (udp + 1) > packet_end) {
        return XDP_DROP;
    }
    struct labrctl_packet* pkt = (void*) (udp + 1);
    if ((void*) (pkt + 1) > packet_end) {
        return XDP_DROP;
    }

    __be32 ip = iph->saddr;
    iph->saddr = iph->daddr;
    iph->daddr = ip;

    __be16 port = udp->source;
    udp->source = udp->dest;
    udp->dest = port;
    udp->check = 0;

    pkt->op = LABRCTL_OP_ACK;
    __builtin_memcpy(pkt->data, &data, sizeof(data));

    return XDP_TX;
}

SEC("xdpfwd")
int xdp_fwd(struct xdp_md* ctx)
{
    void* packet = (void*) (long) ctx->data;
    void* packet_end = (void*) (long) ctx->data_end;

    /*
     * Tailscale sends packets on their own interface,
     * usually called tailscale0. Since this is not TAP but
     * instead a TUN interface, you don't have an ethernet header.
     */
    struct iphdr* iph;
#if TAILSCALE_IFINDEX != 0
    if (ctx->ingress_ifindex == TAILSCALE_IFINDEX) {
        if (packet_end - packet != 60 - ETHHLEN) {
            return XDP_PASS;
        }

        iph = (struct iphdr*) packet;
    } else {
#endif
        /* Strict check, needs VLAN stripping and no IP options */
        if (packet_end - packet != 60) {
            return XDP_PASS;
        }

        struct ethhdr* eth = packet;
        if ((void*) (eth + 1) > packet_end) {
            return XDP_PASS;
        }

        if (eth->h_proto != bpf_htons(ETH_P_IP)) {
            return XDP_PASS;
        }

        iph = (struct iphdr*) (packet + ETHHLEN);
#if TAILSCALE_IFINDEX != 0
    }
#endif

    if ((void*) (iph + 1) > packet_end) {
        return XDP_PASS;
    }

    if (iph->protocol != IPPROTO_UDP) {
        return XDP_PASS;
    }

    __u16 iph_len = iph->ihl * 4;
    struct udphdr* udp = (struct udphdr*) ((void*) iph + iph_len);
    if ((void*) (udp + 1) > packet_end) {
        return XDP_PASS;
    }

    if (udp->dest != bpf_htons(LABRCTL_PORT)) {
        return XDP_PASS;
    }

    void* payload = (void*) (udp + 1);
    if (payload + PACKET_SZ > packet_end) {
        return XDP_PASS;
    }

    struct labrctl_packet* pkt = payload;
    if (pkt->hdr != bpf_htons((LABRCTL_MAGIC << 8) | LABRCTL_VERSION)) {
        return XDP_PASS;
    }

    if (pkt->op == LABRCTL_OP_ACK) {
        return XDP_DROP;
    }

    if (pkt->op == LABRCTL_OP_RESEQ) {
        __sync_lock_test_and_set(&seq_seen, 0);
        return ack_tx(ctx, 0);
    }

    if (pkt->op == LABRCTL_OP_QUIET_SET) {
        xdp_handle_foreign = XDP_DROP;
    }

    if (pkt->op == LABRCTL_OP_QUIET_RESTORE) {
        xdp_handle_foreign = XDP_PASS;
    }

    __u64 expected = (__u8) (pkt->seq - 1);
    if (__sync_val_compare_and_swap(&seq_seen, expected, pkt->seq)) {
        __u64 data = bpf_labrctl_submit(payload, PACKET_SZ);
        return ack_tx(ctx, data);
    }

    if ((__u8) expected == pkt->seq) {
        return ack_tx(ctx, 0);
    }

    return XDP_DROP;
}

char _license[] SEC("license") = "Dual MIT/GPL";
