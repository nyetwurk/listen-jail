#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if.h>

static int (*real_listen)(int, int) = NULL;
static int (*real_bind)(int, const struct sockaddr *, socklen_t) = NULL;
static int log_enabled = 0;

#define LOG(...) do { if (log_enabled) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while (0)

static void __attribute__((constructor)) listen_jail_init(void) {
    const char *debug = getenv("LISTEN_JAIL_DEBUG");
    log_enabled = (debug && *debug);
}

/* IPv4-mapped IPv6 "any": ::ffff:0.0.0.0 */
static const unsigned char in6addr_v4mapped_any[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0
};

/* Returns 1 if addr is INADDR_ANY, in6addr_any, or ::ffff:0.0.0.0; 0 otherwise. Logs skip reason on 0. */
static int is_addr_any(int fd, const struct sockaddr *addr, socklen_t addrlen, const char *ctx) {
    if (addr->sa_family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *in4 = (const struct sockaddr_in *)addr;
        if (in4->sin_addr.s_addr == INADDR_ANY) return 1;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &in4->sin_addr, buf, sizeof(buf));
        LOG("[listen-jail] %s(fd=%d) skip: %s to %s (not 0.0.0.0)\n",
            ctx, fd, ctx[0] == 'b' ? "binding" : "bound", buf);
        return 0;
    }
    if (addr->sa_family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)addr;
        if (memcmp(&in6->sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0) return 1;
        if (memcmp(&in6->sin6_addr, in6addr_v4mapped_any, sizeof(in6addr_v4mapped_any)) == 0)
            return 1;
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf));
        LOG("[listen-jail] %s(fd=%d) skip: %s to %s (not :: or ::ffff:0.0.0.0)\n",
            ctx, fd, ctx[0] == 'b' ? "binding" : "bound", buf);
        return 0;
    }
    LOG("[listen-jail] %s(fd=%d) skip: family %d (not inet/inet6)\n",
        ctx, fd, addr->sa_family);
    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!real_bind) real_bind = dlsym(RTLD_NEXT, "bind");

    const char *bind_ip = getenv("BIND_IP");
    if (!bind_ip) {
        return real_bind(sockfd, addr, addrlen);
    }

    if (!is_addr_any(sockfd, addr, addrlen, "bind"))
        return real_bind(sockfd, addr, addrlen);

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in mod = *(const struct sockaddr_in *)addr;
        if (inet_pton(AF_INET, bind_ip, &mod.sin_addr) != 1) {
            LOG("[listen-jail] bind(fd=%d) skip: BIND_IP invalid for IPv4: %s\n",
                sockfd, bind_ip);
            return real_bind(sockfd, addr, addrlen);
        }
        LOG("[listen-jail] bind(fd=%d) 0.0.0.0 -> %s\n", sockfd, bind_ip);
        return real_bind(sockfd, (const struct sockaddr *)&mod, sizeof(mod));
    }

    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 mod = *(const struct sockaddr_in6 *)addr;
        struct in_addr ip4;
        if (inet_pton(AF_INET, bind_ip, &ip4) == 1) {
            memcpy(&mod.sin6_addr, in6addr_v4mapped_any, sizeof(mod.sin6_addr));
            memcpy(&mod.sin6_addr.s6_addr[12], &ip4, sizeof(ip4));
            LOG("[listen-jail] bind(fd=%d) [::ffff:0.0.0.0] -> ::ffff:%s\n", sockfd, bind_ip);
        } else if (inet_pton(AF_INET6, bind_ip, &mod.sin6_addr) == 1) {
            LOG("[listen-jail] bind(fd=%d) [::] -> %s\n", sockfd, bind_ip);
        } else {
            LOG("[listen-jail] bind(fd=%d) skip: BIND_IP invalid: %s\n", sockfd, bind_ip);
            return real_bind(sockfd, addr, addrlen);
        }
        return real_bind(sockfd, (const struct sockaddr *)&mod, sizeof(mod));
    }

    return real_bind(sockfd, addr, addrlen);
}

int listen(int sockfd, int backlog) {
    if (!real_listen) real_listen = dlsym(RTLD_NEXT, "listen");

    const char *iface = getenv("BIND_INTERFACE");
    if (iface) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);
        if (getsockname(sockfd, (struct sockaddr *)&addr, &addrlen) != 0) {
            LOG("[listen-jail] listen(fd=%d) skip: getsockname failed\n", sockfd);
        } else if (is_addr_any(sockfd, (const struct sockaddr *)&addr, addrlen, "listen")) {
            size_t len = strnlen(iface, IFNAMSIZ) + 1;
            int ret = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, iface, len);
            LOG("[listen-jail] listen(fd=%d) SO_BINDTODEVICE=%s -> %s\n",
                sockfd, iface, ret == 0 ? "ok" : "failed");
        }
    }

    return real_listen(sockfd, backlog);
}
