// A Linux multicast pitfall (Section 15.2): a socket bound to INADDR_ANY:port receives
// datagrams for EVERY group that ANY socket on the host has joined on that port, not just
// the group it joined itself, unless IP_MULTICAST_ALL is turned off. Tested on loopback.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <initializer_list>
#include "mcast.hpp"

constexpr unsigned short kPort = 31005;               // both groups use the same port

int open_any(const char* group, int multicast_all) {  // bound to 0.0.0.0:kPort
    const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    const sockaddr_in a = mc::addr("0.0.0.0", kPort);
    mc::check(bind(fd, reinterpret_cast<const sockaddr*>(&a), sizeof a) == 0, "bind");
    ip_mreq m{};
    inet_pton(AF_INET, group, &m.imr_multiaddr);
    inet_pton(AF_INET, "127.0.0.1", &m.imr_interface);
    mc::check(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m) == 0, "join");
    mc::check(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_ALL, &multicast_all,
                         sizeof multicast_all) == 0, "IP_MULTICAST_ALL");
    return fd;
}

int count(int fd) {                                   // datagrams waiting on fd
    char buf[64];
    int n = 0;
    while (recv(fd, buf, sizeof buf, MSG_DONTWAIT) > 0) ++n;
    return n;
}

int main() {
    const int tx = mc::open_tx("127.0.0.1");
    for (int all : {1, 0}) {                          // 1 is the Linux default
        const int x = open_any("239.255.1.1", all);   // wants group .1 only
        const int y = open_any("239.255.1.2", all);   // wants group .2 only
        const int z = mc::open_rx("239.255.1.1", kPort, "127.0.0.1");   // mcast.hpp
        for (const char* g : {"239.255.1.1", "239.255.1.2"}) {
            const sockaddr_in d = mc::addr(g, kPort);
            sendto(tx, g, 12, 0, reinterpret_cast<const sockaddr*>(&d), sizeof d);
        }
        std::printf("IP_MULTICAST_ALL=%d: any-bound(.1) got %d, any-bound(.2) got %d, "
                    "group-bound(.1) got %d\n", all, count(x), count(y), count(z));
        close(x); close(y); close(z);
    }
}
