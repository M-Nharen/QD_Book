// Checks the numeric answers to the Chapter 15 practice problems, and replays the
// arrival sequence of P15.4 through the arbiter of Listing 15.3.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "feed_arbiter.hpp"
#include "feed_build.hpp"

int main() {
    // ---- P15.1 wire time: frame + 8 preamble/SFD + 12 inter-frame gap
    auto wire_ns = [](int frame_bytes, double gbps) {
        return (frame_bytes + 20) * 8 / gbps;
    };
    const int udp100 = 14 + 20 + 8 + 100 + 4;
    std::printf("P15.1 (a) 64 B frame at 10G: %.1f ns, %.2f Mpps\n", wire_ns(64, 10),
                1e3 / wire_ns(64, 10));
    std::printf("      (b) 100 B payload: frame %d B, %.1f ns at 10G, %.2f ns at 25G\n",
                udp100, wire_ns(udp100, 10), wire_ns(udp100, 25));
    std::printf("      (c) 1518 B frame at 10G: %.1f ns\n", wire_ns(1518, 10));
    std::printf("      (d) smallest unpadded UDP payload: %d B\n", 64 - 4 - 14 - 20 - 8);

    // ---- P15.2 multicast MAC: 01:00:5e + low 23 bits of the group address
    auto mac = [](unsigned a, unsigned b, unsigned c, unsigned d) {
        char s[32];
        std::snprintf(s, sizeof s, "01:00:5e:%02x:%02x:%02x", b & 0x7f, c, d);
        (void)a;
        return std::string(s);
    };
    std::printf("P15.2 239.1.1.1 -> %s, 224.129.1.1 -> %s,\n      239.129.1.1 -> %s; "
                "%d groups per MAC\n", mac(239, 1, 1, 1).c_str(),
                mac(224, 129, 1, 1).c_str(), mac(239, 129, 1, 1).c_str(), 1 << (28 - 23));

    // ---- P15.4 replay: packets (seq, count); A loses P3 and P6, B loses P6 and P7
    struct Arr { char line; std::uint64_t seq; std::uint16_t count; };
    const std::vector<Arr> arrivals = {{'A', 1, 2}, {'A', 3, 1}, {'B', 1, 2}, {'A', 6, 2},
                                       {'B', 3, 1}, {'A', 8, 1}, {'B', 4, 2}, {'B', 6, 2},
                                       {'A', 12, 1}, {'B', 8, 1}};
    for (unsigned hold : {2u, 4u}) {
        auto arb = std::make_unique<feed::Arbiter<8>>(1, hold);
        std::string log;
        auto deliver = [&](std::uint64_t s, std::span<const std::byte>) {
            log += std::to_string(s) + " ";
        };
        auto gap = [&](std::uint64_t f, std::uint64_t t) {
            log += "[gap " + std::to_string(f) + "-" + std::to_string(t - 1) + "] ";
        };
        for (const Arr& a : arrivals) {
            std::byte pkt[feed::kMaxPacket], m[1]{};
            const std::vector<std::span<const std::byte>> msgs(a.count, m);
            const std::size_t n = feed::build(pkt, a.seq, msgs);
            arb->on_packet({pkt, n}, deliver, gap);
        }
        std::byte hb[feed::kMaxPacket];                 // heartbeat: next seq is 13
        arb->on_packet({hb, feed::build(hb, 13, {})}, deliver, gap);
        arb->flush(deliver, gap);
        std::printf("P15.4 hold %u: %s(dups %llu)\n", hold, log.c_str(),
                    static_cast<unsigned long long>(arb->stats().dup_pkts));
    }

    // ---- P15.5 receive buffer
    const double rate = 300'000, stall = 4e-3, truesize = 1280, drain = 1e6;
    const double queued = rate * stall;
    std::printf("P15.5 queued %.0f; SO_RCVBUF needed %.0f B (setsockopt %.0f);\n"
                "      backlog cleared in %.3f ms\n", queued, queued * truesize,
                queued * truesize / 2, 1e3 * queued / (drain - rate));

    // ---- P15.6 fibre vs microwave
    const double c = 299'792'458.0;
    const double fibre = 1250e3 * 1.47 / c, radio = 1020e3 * 1.0003 / c, ideal = 1000e3 / c;
    std::printf("P15.6 fibre %.3f ms, microwave %.3f ms, difference %.3f ms one-way\n"
                "      (%.3f ms RTT); great circle at c %.3f ms\n", fibre * 1e3,
                radio * 1e3, (fibre - radio) * 1e3, 2 * (fibre - radio) * 1e3, ideal * 1e3);

    // ---- P15.7 FPGA beats: 8 bytes per 6.4 ns beat
    const int last_byte = 49, beats = last_byte / 8 + 1;
    std::printf("P15.7 field complete after %d beats = %.1f ns; 200 B frame at line rate "
                "%.0f ns\n", beats, beats * 6.4, 200 * 0.8);

    // ---- P15.8 A/B loss arithmetic
    const double p = 1e-4, pps = 500'000;
    std::printf("P15.8 single-line losses %.2f /s; true gaps %.4f /s (one per %.0f s)\n",
                2 * pps * p * (1 - p), pps * p * p, 1 / (pps * p * p));

    // ---- P15.10 PTP
    const double t1 = 1'000'000, t2 = 1'000'900, t3 = 1'050'000, t4 = 1'050'300;
    const double offset = ((t2 - t1) - (t4 - t3)) / 2, delay = ((t2 - t1) + (t4 - t3)) / 2;
    const double true_offset = (t2 - t1) - 700;          // forward path really 700 ns
    std::printf("P15.10 offset %.0f ns, delay %.0f ns; true offset %.0f, error %.0f ns\n",
                offset, delay, true_offset, offset - true_offset);

    // ---- P15.11 free-running u32 ring indices
    const std::uint32_t prod = 7, cons = 4'294'967'291u;
    std::printf("P15.11 entries available: %u; slot of next entry: %u\n", prod - cons,
                cons & 2047u);
}
