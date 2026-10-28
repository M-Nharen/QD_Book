// End-to-end test of A/B arbitration over real multicast sockets on the loopback interface
// (Section 15.2). The sender publishes the same packets on two groups (lines A and B),
// drops 3% of packets on each line independently, delays line B by three packets and
// swaps a few adjacent packets on line A. The receiver must deliver every message exactly
// once, in order, and report as gaps exactly the messages whose packets BOTH lines lost.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>
#include "feed_arbiter.hpp"
#include "feed_build.hpp"
#include "mcast.hpp"

int main() {
    constexpr const char* kIf = "127.0.0.1";          // loopback stands in for the NIC
    const int rx_a = mc::open_rx("239.255.0.1", 31001, kIf, 1 << 20);
    const int rx_b = mc::open_rx("239.255.0.2", 31002, kIf, 1 << 20);
    const int tx = mc::open_tx(kIf);
    const sockaddr_in dst[2] = {mc::addr("239.255.0.1", 31001),
                                mc::addr("239.255.0.2", 31002)};

    struct Plan { std::uint64_t seq; std::uint16_t count; bool drop[2]; };
    constexpr int kPackets = 20000, kLagB = 3;
    std::mt19937_64 rng(15);
    std::uniform_real_distribution<double> u(0, 1);
    std::vector<Plan> plan;
    std::vector<std::uint64_t> expect_lost;           // messages lost on BOTH lines
    std::uint64_t seq = 1;
    for (int i = 0; i < kPackets; ++i) {
        const auto count = static_cast<std::uint16_t>(1 + rng() % 4);
        Plan p{seq, count, {u(rng) < 0.03, u(rng) < 0.03}};
        if (p.drop[0] && p.drop[1])
            for (std::uint64_t s = p.seq; s < p.seq + count; ++s) expect_lost.push_back(s);
        seq += p.count;
        plan.push_back(p);
    }
    std::vector<int> order_a(kPackets);               // line A's send order: a few swaps
    for (int i = 0; i < kPackets; ++i) order_a[i] = i;
    for (int i = 0; i + 1 < kPackets; ++i)
        if (u(rng) < 0.005) std::swap(order_a[i], order_a[i + 1]);

    auto send = [&](int line, std::uint64_t first, std::uint16_t count) {
        std::byte msg[4][16]{}, pkt[feed::kMaxPacket];
        std::span<const std::byte> spans[4];
        for (int m = 0; m < count; ++m) {             // payload: the message's own seq
            feed::store_be64(msg[m], first + m);
            spans[m] = msg[m];
        }
        const std::size_t n = feed::build(pkt, first, {spans, count});
        mc::check(sendto(tx, pkt, n, 0, reinterpret_cast<const sockaddr*>(&dst[line]),
                         sizeof dst[line]) == static_cast<ssize_t>(n), "sendto");
    };

    auto arb = std::make_unique<feed::Arbiter<>>(1, 32);
    std::uint64_t expect_next = 1, order_errors = 0, rx_pkts[2] = {0, 0};
    std::vector<std::uint64_t> reported_lost;
    auto deliver = [&](std::uint64_t s, std::span<const std::byte> m) {
        if (s != expect_next || feed::load_be64(m.data()) != s) ++order_errors;
        expect_next = s + 1;
    };
    auto gap = [&](std::uint64_t from, std::uint64_t to) {
        for (std::uint64_t s = from; s < to; ++s) reported_lost.push_back(s);
        expect_next = to;                             // recovery would fill [from, to)
    };
    auto drain = [&](int line, int fd) {
        std::byte buf[2048];
        ssize_t n;
        while ((n = recv(fd, buf, sizeof buf, MSG_DONTWAIT)) > 0) {
            ++rx_pkts[line];
            arb->on_packet({buf, static_cast<std::size_t>(n)}, deliver, gap);
        }
    };

    for (int t = 0; t < kPackets + kLagB; ++t) {
        if (t < kPackets && !plan[order_a[t]].drop[0])
            send(0, plan[order_a[t]].seq, plan[order_a[t]].count);
        if (t >= kLagB && !plan[t - kLagB].drop[1])
            send(1, plan[t - kLagB].seq, plan[t - kLagB].count);
        drain(0, rx_a);
        drain(1, rx_b);
    }
    send(0, seq, 0);                                  // heartbeats carry the next seq,
    send(1, seq, 0);                                  // so trailing losses are seen too
    drain(0, rx_a);
    drain(1, rx_b);
    arb->flush(deliver, gap);

    const auto& st = arb->stats();
    const bool ok = order_errors == 0 && reported_lost == expect_lost &&
                    st.delivered + st.lost_msgs == seq - 1;
    std::printf("published %llu messages in %d packets per line\n"
                "received  A %llu packets, B %llu packets\n"
                "delivered %llu messages (order errors %llu); duplicates dropped %llu; "
                "held %llu\n"
                "gaps      %llu declared, covering %llu messages; lost on both lines %zu"
                " -> %s\n",
                static_cast<unsigned long long>(seq - 1), kPackets,
                static_cast<unsigned long long>(rx_pkts[0]),
                static_cast<unsigned long long>(rx_pkts[1]),
                static_cast<unsigned long long>(st.delivered),
                static_cast<unsigned long long>(order_errors),
                static_cast<unsigned long long>(st.dup_pkts + st.late_pkts),
                static_cast<unsigned long long>(st.held_pkts),
                static_cast<unsigned long long>(st.gaps),
                static_cast<unsigned long long>(st.lost_msgs), expect_lost.size(),
                ok ? "match: OK" : "MISMATCH");
    return ok ? 0 : 1;
}
