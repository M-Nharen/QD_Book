// Small helpers for the multicast listings: a receiving socket that joins one group on
// one interface, and a sending socket bound to an interface (Section 15.2). Errors are
// fatal.
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

namespace mc {

inline void check(bool ok, const char* what) {
    if (!ok) { std::perror(what); std::exit(1); }
}

inline sockaddr_in addr(const char* ip, unsigned short port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    check(inet_pton(AF_INET, ip, &a.sin_addr) == 1, "inet_pton");
    return a;
}

// Receive `group:port` arriving on the interface that owns address `ifaddr`.
inline int open_rx(const char* group, unsigned short port, const char* ifaddr,
                   int rcvbuf_bytes = 0, bool nonblock = true) {
    const int fd = socket(AF_INET, SOCK_DGRAM | (nonblock ? SOCK_NONBLOCK : 0), 0);
    check(fd >= 0, "socket");
    const int one = 1, zero = 0;
    check(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) == 0, "REUSEADDR");
    const sockaddr_in a = addr(group, port);            // bind the GROUP, not INADDR_ANY
    check(bind(fd, reinterpret_cast<const sockaddr*>(&a), sizeof a) == 0, "bind");
    ip_mreq m{};
    m.imr_multiaddr = a.sin_addr;
    check(inet_pton(AF_INET, ifaddr, &m.imr_interface) == 1, "ifaddr");
    check(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m) == 0, "join");
    check(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero) == 0, "ALL");
    if (rcvbuf_bytes > 0)                               // FORCE ignores rmem_max; needs
        check(setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE,  // CAP_NET_ADMIN
                         &rcvbuf_bytes, sizeof rcvbuf_bytes) == 0, "RCVBUFFORCE");
    return fd;
}

// Send multicast out of the interface that owns `ifaddr`, scope limited by `ttl`.
inline int open_tx(const char* ifaddr, unsigned char ttl = 1) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    check(fd >= 0, "socket");
    in_addr ifa{};
    check(inet_pton(AF_INET, ifaddr, &ifa) == 1, "ifaddr");
    check(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &ifa, sizeof ifa) == 0, "MCAST_IF");
    check(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl) == 0, "TTL");
    return fd;
}

}  // namespace mc
