#include <linux/types.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <stdatomic.h>

#include "labrctl_ctl.h"

#define ETHHLEN (sizeof(struct ethhdr))
#define UDPHLEN (sizeof(struct udphdr))

extern int bpf_labrctl_submit(void* data, size_t data__sz) __ksym;

static _Atomic __u64 seq_seen = 0;

static __always_inline int ack_tx(
    struct xdp_md* ctx,
    struct labrctl_packet* pkt
)
{
    struct ethhdr* eth = (void*) (long) ctx->data;
    struct iphdr* iph = (struct iphdr*) (eth + 1);
    struct udphdr* udp = (struct udphdr*) (iph + 1);

    __u8 mac[ETH_ALEN];
    __builtin_memcpy(mac, eth->h_source, ETH_ALEN);
    __builtin_memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
    __builtin_memcpy(eth->h_dest, mac, ETH_ALEN);

    __be32 ip = iph->saddr;
    iph->saddr = iph->daddr;
    iph->daddr = ip;

    __be16 port = udp->source;
    udp->source = udp->dest;
    udp->dest = port;
    udp->check = 0;

    pkt->op = LABRCTL_OP_ACK;

    return XDP_TX;
}

SEC("xdpfwd")
int xdp_fwd(struct xdp_md* ctx)
{
    void* packet = (void*) (long) ctx->data;
    void* packet_end = (void*) (long) ctx->data_end;

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

    struct iphdr* iph = (struct iphdr*) (packet + ETHHLEN);
    if ((void*) (iph + 1) > packet_end) {
        return XDP_PASS;
    }

    if (iph->protocol != IPPROTO_UDP) {
        return XDP_PASS;
    }

    __u16 iph_len = iph->ihl * 4;
    struct udphdr* udp = (struct udphdr*) (packet + ETHHLEN + iph_len);
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

    __u64 expected = (__u8) (pkt->seq - 1);
    if (atomic_compare_exchange_strong(&seq_seen, &expected, pkt->seq)) {
        bpf_labrctl_submit(payload, PACKET_SZ);
        return ack_tx(ctx, pkt);
    }

    if ((__u8) expected == pkt->seq) {
        return ack_tx(ctx, pkt);
    }

    return XDP_DROP;
}

char _license[] SEC("license") = "Dual MIT/GPL";
