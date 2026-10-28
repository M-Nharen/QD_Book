// Showdown harness (Section 15.6). A sender thread on CPU 0 publishes a sequenced feed
// (feed_protocol.hpp) to a multicast group on loopback; the receiver under test runs on
// CPU 1 and hands each message to on_message(). Phase 1 measures latency: 20,000 packets,
// one every 20 us, each stamped with its send time. Phase 2 measures loss under bursts:
// 50 bursts of 2,000 back-to-back packets of 4 messages, 2 ms apart.
#pragma once
#include <pthread.h>
#include <sched.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>
#include <vector>
#include "feed_build.hpp"
#include "mcast.hpp"

inline std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

struct Harness {
    static constexpr const char* kGroup = "239.255.5.1";
    static constexpr unsigned short kPort = 31040;
    static constexpr int kLatPackets = 20000, kBursts = 50, kBurstPackets = 2000;

    std::atomic<bool> ready{false}, done{false};
    std::vector<std::int64_t> lat;                   // receiver thread only
    std::uint64_t delivered = 0, published = 0;
    long gaps_seen = -1;                             // -1: receiver does not check

    Harness() { lat.reserve(kLatPackets); }

    void on_message(std::uint64_t, std::span<const std::byte> m) {
        ++delivered;
        if (const std::int64_t sent = static_cast<std::int64_t>(feed::load_be64(m.data())))
            lat.push_back(now_ns() - sent);           // phase-1 messages carry a stamp
    }

    void run(const char* name, const std::function<void(Harness&)>& receiver) {
        std::thread rx([&] { pin(1); receiver(*this); });
        pin(0);
        while (!ready.load(std::memory_order_acquire)) {}
        const int tx = mc::open_tx("127.0.0.1");
        const sockaddr_in dst = mc::addr(kGroup, kPort);
        std::uint64_t seq = 1;
        auto send = [&](int msgs, std::int64_t stamp) {
            std::byte payload[4][32]{}, pkt[feed::kMaxPacket];
            std::span<const std::byte> spans[4];
            for (int i = 0; i < msgs; ++i) {
                feed::store_be64(payload[i], static_cast<std::uint64_t>(stamp));
                spans[i] = payload[i];
            }
            const std::size_t n = feed::build(pkt, seq, {spans, std::size_t(msgs)});
            sendto(tx, pkt, n, 0, reinterpret_cast<const sockaddr*>(&dst), sizeof dst);
            seq += msgs;
        };
        std::int64_t next = now_ns();
        for (int i = 0; i < kLatPackets; ++i) {       // phase 1: paced, 1 message each
            next += 20'000;
            while (now_ns() < next) {}
            send(1, now_ns());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const std::uint64_t burst_first = seq;
        for (int b = 0; b < kBursts; ++b) {           // phase 2: bursts, 4 messages each
            for (int i = 0; i < kBurstPackets; ++i) send(4, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done.store(true, std::memory_order_release);
        rx.join();
        close(tx);
        published = seq - 1;
        report(name, burst_first);
    }

private:
    void report(const char* name, std::uint64_t burst_first_seq) {
        std::sort(lat.begin(), lat.end());
        auto pct = [&](double p) {
            return lat.empty() ? 0.0 : lat[std::size_t(p * double(lat.size() - 1))] / 1e3;
        };
        const std::uint64_t burst_msgs = published - burst_first_seq + 1;
        std::printf("%s\n  latency, %zu of %d paced messages: p50 %.1f  p99 %.1f  "
                    "p99.9 %.1f  max %.1f us\n", name, lat.size(), kLatPackets, pct(0.5),
                    pct(0.99), pct(0.999), lat.empty() ? 0.0 : lat.back() / 1e3);
        std::printf("  bursts: %llu messages published; lost %llu (%.2f%%); ",
                    static_cast<unsigned long long>(burst_msgs),
                    static_cast<unsigned long long>(published - delivered),
                    100.0 * double(published - delivered) / double(burst_msgs));
        if (gaps_seen < 0) std::printf("receiver cannot tell\n");
        else std::printf("receiver reported %ld gaps\n", gaps_seen);
    }
};
