// book: flags=-pthread
// Where does a datagram's time go inside the kernel? (Section 15.5). SO_TIMESTAMPING asks
// the kernel for software timestamps as the packet leaves the IP stack for the driver (TX)
// and as the driver hands it to the stack (RX). With the application's own clock reads
// that splits one loopback transfer into three legs. Hardware stamps need a NIC with a
// PTP clock; `ethtool -T <if>` shows what an interface supports. tsan-clean.
#include <linux/net_tstamp.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>
#include "mcast.hpp"

static std::int64_t ns(const timespec& t) { return t.tv_sec * 1'000'000'000LL + t.tv_nsec; }
static std::int64_t now_ns() {
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);                // software stamps use CLOCK_REALTIME
    return ns(t);
}

// Reads the SCM_TIMESTAMPING control message: ts[0] = software, ts[2] = raw hardware.
static bool stamp(msghdr& m, timespec out[3]) {
    for (cmsghdr* c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_TIMESTAMPING) {
            std::memcpy(out, CMSG_DATA(c), 3 * sizeof(timespec));
            return true;
        }
    return false;
}

int main() {
    constexpr int kN = 5000;
    const int rx = mc::open_rx("239.255.3.1", 31020, "127.0.0.1");
    const int tx = mc::open_tx("127.0.0.1");
    const int rx_flags = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE |
                         SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    const int tx_flags = SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE |
                         SOF_TIMESTAMPING_OPT_ID | SOF_TIMESTAMPING_OPT_TSONLY;
    mc::check(setsockopt(rx, SOL_SOCKET, SO_TIMESTAMPING, &rx_flags, sizeof rx_flags) == 0,
              "rx SO_TIMESTAMPING");
    mc::check(setsockopt(tx, SOL_SOCKET, SO_TIMESTAMPING, &tx_flags, sizeof tx_flags) == 0,
              "tx SO_TIMESTAMPING");

    std::vector<std::int64_t> app_tx(kN), drv_tx(kN), drv_rx(kN), app_rx(kN);
    std::atomic<int> received{0};
    int hw_rx = 0;
    std::thread receiver([&] {                        // busy-polls, so no wake-up cost
        for (int i = 0; i < kN;) {
            std::int64_t sent;
            iovec iov{&sent, sizeof sent};
            alignas(cmsghdr) char ctrl[256];
            msghdr m{};
            m.msg_iov = &iov;
            m.msg_iovlen = 1;
            m.msg_control = ctrl;
            m.msg_controllen = sizeof ctrl;
            if (recvmsg(rx, &m, MSG_DONTWAIT) < 0) continue;
            const std::int64_t t = now_ns();
            timespec ts[3]{};
            stamp(m, ts);
            drv_rx[i] = ns(ts[0]);
            hw_rx += ns(ts[2]) != 0;                  // stays 0 without NIC support
            app_rx[i] = t;
            received.store(++i, std::memory_order_release);
        }
    });

    const sockaddr_in dst = mc::addr("239.255.3.1", 31020);
    for (int i = 0; i < kN; ++i) {
        while (received.load(std::memory_order_acquire) < i) {}   // one in flight
        app_tx[i] = now_ns();
        sendto(tx, &app_tx[i], sizeof app_tx[i], 0,
               reinterpret_cast<const sockaddr*>(&dst), sizeof dst);
        for (;;) {                                    // TX stamp: the socket error queue
            alignas(cmsghdr) char ctrl[256];
            msghdr m{};
            m.msg_control = ctrl;
            m.msg_controllen = sizeof ctrl;
            timespec ts[3]{};
            if (recvmsg(tx, &m, MSG_ERRQUEUE | MSG_DONTWAIT) >= 0 && stamp(m, ts)) {
                drv_tx[i] = ns(ts[0]);
                break;
            }
        }
    }
    receiver.join();

    auto median = [&](const std::vector<std::int64_t>& to,
                      const std::vector<std::int64_t>& from) {
        std::vector<std::int64_t> d(kN);
        for (int i = 0; i < kN; ++i) d[i] = to[i] - from[i];
        std::nth_element(d.begin(), d.begin() + kN / 2, d.end());
        return static_cast<long long>(d[kN / 2]);
    };
    std::printf("%d datagrams over loopback, medians (ns):\n", kN);
    std::printf("  app send -> driver TX stamp   %6lld  (syscall, UDP, IP, routing)\n",
                median(drv_tx, app_tx));
    std::printf("  driver TX -> driver RX stamp  %6lld  (loopback 'wire')\n",
                median(drv_rx, drv_tx));
    std::printf("  driver RX -> app has data     %6lld  (IP, UDP, socket queue, recvmsg)\n",
                median(app_rx, drv_rx));
    std::printf("  total                         %6lld\n", median(app_rx, app_tx));
    std::printf("hardware RX stamps received: %d of %d\n", hw_rx, kN);
}
