// What the kernel does when a UDP receiver falls behind (Section 15.3): a burst arrives
// while the application is not reading; datagrams that do not fit in the socket's receive
// buffer are silently dropped. Three independent witnesses: sequence numbers, the
// per-socket drop counter (SO_RXQ_OVFL), and the host-wide counter in /proc/net/snmp.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include "mcast.hpp"

long udp_rcvbuf_errors() {             // 2nd "Udp:" line of /proc/net/snmp, 5th number
    std::ifstream f("/proc/net/snmp");
    std::string tag;
    long v[5] = {};
    for (int udp_lines = 0; f >> tag;)
        if (tag == "Udp:" && ++udp_lines == 2) {          // InDatagrams NoPorts InErrors
            for (long& x : v) f >> x;                     // OutDatagrams RcvbufErrors ...
            return v[4];
        }
    return -1;
}

void trial(int requested_bytes) {
    constexpr std::uint32_t kBurst = 5000;
    const int rx = mc::open_rx("239.255.2.1", 31010, "127.0.0.1", requested_bytes);
    const int tx = mc::open_tx("127.0.0.1");
    const int one = 1;
    setsockopt(rx, SOL_SOCKET, SO_RXQ_OVFL, &one, sizeof one);
    int actual = 0;
    socklen_t len = sizeof actual;
    getsockopt(rx, SOL_SOCKET, SO_RCVBUF, &actual, &len);
    const sockaddr_in dst = mc::addr("239.255.2.1", 31010);
    const long errors_before = udp_rcvbuf_errors();

    char pkt[100]{};                                      // 100-byte datagrams
    int got = 0, gaps = 0;
    std::uint32_t expect = 0, socket_drops = 0;
    auto send = [&](std::uint32_t seq) {
        std::memcpy(pkt, &seq, sizeof seq);
        sendto(tx, pkt, sizeof pkt, 0, reinterpret_cast<const sockaddr*>(&dst), sizeof dst);
    };
    auto drain = [&] {
        alignas(cmsghdr) char ctrl[CMSG_SPACE(sizeof(std::uint32_t))];
        iovec iov{pkt, sizeof pkt};
        msghdr m{nullptr, 0, &iov, 1, ctrl, sizeof ctrl, 0};
        for (; recvmsg(rx, &m, MSG_DONTWAIT) >= 0; m.msg_controllen = sizeof ctrl) {
            for (cmsghdr* c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
                if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_RXQ_OVFL)
                    std::memcpy(&socket_drops, CMSG_DATA(c), sizeof socket_drops);
            std::uint32_t seq;
            std::memcpy(&seq, pkt, sizeof seq);
            gaps += seq != expect;                        // what a feed handler sees
            expect = seq + 1;
            ++got;
        }
    };
    for (std::uint32_t seq = 0; seq < kBurst; ++seq) send(seq);   // application "stalled"
    drain();                                              // wakes up, drains the queue
    const int gaps_after_burst = gaps;
    send(kBurst);                                         // the next datagram arrives
    drain();
    std::printf("SO_RCVBUF %8d: kept %4d of %u; gaps %d, then %d; SO_RXQ_OVFL %4u; "
                "snmp +%ld\n", actual, got - 1, kBurst, gaps_after_burst, gaps,
                socket_drops, udp_rcvbuf_errors() - errors_before);
    close(rx);
    close(tx);
}

int main() {
    trial(0);                                             // net.core.rmem_default
    trial(1 << 20);                                       // asks for 1 MiB (gets 2 MiB)
    trial(8 << 20);                                       // asks for 8 MiB (gets 16 MiB)
}
