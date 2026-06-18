#include "labrctl_ctl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

const char* labrctl_ctl_version(void)
{
    return LIBLABRCTL_VERSION;
}

int labrctl_ctl_open(
    struct labrctl_ctl_client* c,
    const char* server,
    uint16_t port,
    unsigned timeout_ms,
    uint8_t retries
)
{
    if (c == NULL || server == NULL) {
        return -EINVAL;
    }

    *c = (struct labrctl_ctl_client) { 0 };
    c->seq = 1;
    c->retries = retries ? retries : 4;
    c->timeout.tv_sec = timeout_ms / 1000;
    c->timeout.tv_usec = (timeout_ms % 1000) * 1000;

    c->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (c->fd < 0) {
        return -errno;
    }

    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(port ? port : LABRCTL_PORT),
    };
    if (inet_pton(AF_INET, server, &sa.sin_addr) != 1) {
        close(c->fd);
        c->fd = -1;
        return -EINVAL;
    }

    if (connect(c->fd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
        int e = -errno;
        close(c->fd);
        c->fd = -1;
        return e;
    }

    int ret = setsockopt(
        c->fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &c->timeout,
        sizeof(c->timeout)
    );
    if (ret < 0) {
        close(c->fd);
        c->fd = -1;
        return ret;
    }
    return 0;
}

void labrctl_ctl_close(struct labrctl_ctl_client* c)
{
    if (c != NULL && c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
}

static void build(
    struct labrctl_ctl_packet* pkt,
    uint8_t op,
    uint8_t seq,
    const uint8_t arg[2],
    const uint8_t data[8]
)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->hdr = htons((LABRCTL_MAGIC << 8) | LABRCTL_VERSION);
    pkt->op = op;
    pkt->seq = seq;
    if (arg != NULL) {
        memcpy(pkt->arg, arg, sizeof(pkt->arg));
    }
    if (data != NULL) {
        memcpy(pkt->data, data, sizeof(pkt->data));
    }
}

static int exchange(
    struct labrctl_ctl_client* c,
    uint8_t op,
    uint8_t seq,
    const uint8_t arg[2],
    const uint8_t data[8],
    struct labrctl_ctl_packet* reply
)
{
    struct labrctl_ctl_packet pkt;
    build(&pkt, op, seq, arg, data);

    if (send(c->fd, &pkt, sizeof(pkt), 0) < 0) {
        return -errno;
    }

    ssize_t n = recv(c->fd, reply, sizeof(*reply), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -errno;
    }
    if ((size_t) n < sizeof(*reply) ||
        reply->hdr != htons((LABRCTL_MAGIC << 8) | LABRCTL_VERSION)) {
        return 0;
    }
    return 1;
}

int labrctl_ctl_command(
    struct labrctl_ctl_client* c,
    uint8_t op,
    const uint8_t arg[2],
    const uint8_t data[8]
)
{
    if (c == NULL || c->fd < 0) {
        return -EINVAL;
    }

    for (unsigned attempt = 0; attempt < c->retries; attempt++) {
        struct labrctl_ctl_packet reply;
        int r = exchange(c, op, c->seq, arg, data, &reply);
        if (r < 0) {
            return r;
        }
        if (r == 0) {
            continue;
        }
        if (reply.op != LABRCTL_OP_ACK) {
            return -EBADMSG;
        }
        if (reply.seq != c->seq) {
            continue;
        }
        int acked = c->seq;
        c->seq++;
        return acked;
    }

    return -ETIMEDOUT;
}

int labrctl_ctl_resync(struct labrctl_ctl_client* c)
{
    int ret = labrctl_ctl_command(c, LABRCTL_OP_RESEQ, NULL, NULL);
    c->seq = 0;

    return ret;
}
