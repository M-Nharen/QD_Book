// Code showdown, naive version (Section 13.9). A position table of 2^22 records (128 MiB)
// is updated by 2^18 random events, and every event is timed. The table "is zeroed at
// startup" with calloc, the thread runs wherever the scheduler puts it, and progress is
// logged with stdio from inside the loop.
#include <sched.h>
#include <sys/resource.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Record {                                    // 32 bytes: two per cache line
    std::int64_t position, notional, last_px;
    std::uint64_t updates;
};
constexpr std::uint32_t kRecords = 1u << 22, kEvents = 1u << 18;

int main() {
    auto* table = static_cast<Record*>(std::calloc(kRecords, sizeof(Record)));
    std::vector<std::uint32_t> lat_ns(kEvents);
    FILE* log = std::fopen("/dev/null", "w");
    if (!table || !log) return 1;

    rusage r0{}, r1{};
    getrusage(RUSAGE_SELF, &r0);
    std::uint64_t x = 88172645463325252ull;       // xorshift64 event generator
    int cpu = sched_getcpu(), migrations = 0;
    for (std::uint32_t i = 0; i < kEvents; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        Record& r = table[x % kRecords];
        const std::int64_t qty = std::int64_t(x >> 40 & 255) - 128, px = 10000 + (x & 1023);
        r.position += qty;
        r.notional += qty * px;
        r.last_px = px;
        ++r.updates;
        if ((i & 1023) == 0) {                     // "cheap" progress logging
            std::fprintf(log, "event %u pos %lld\n", i, static_cast<long long>(r.position));
            std::fflush(log);
        }
        const auto t1 = std::chrono::steady_clock::now();
        lat_ns[i] = static_cast<std::uint32_t>((t1 - t0).count());
        if (const int c = sched_getcpu(); c != cpu) { ++migrations; cpu = c; }
    }
    getrusage(RUSAGE_SELF, &r1);

    std::int64_t check = 0;
    for (std::uint32_t k = 0; k < kRecords; ++k) check += table[k].position;
    std::sort(lat_ns.begin(), lat_ns.end());
    auto pct = [&](double p) { return lat_ns[std::size_t(p * (kEvents - 1))]; };
    std::printf("naive: p50 %u  p90 %u  p99 %u  p99.9 %u  max %u ns\n", pct(0.5),
                pct(0.9), pct(0.99), pct(0.999), lat_ns.back());
    std::printf("during the loop: %ld minor faults, %ld voluntary / %ld involuntary "
                "switches, %d migrations\n", r1.ru_minflt - r0.ru_minflt,
                r1.ru_nvcsw - r0.ru_nvcsw, r1.ru_nivcsw - r0.ru_nivcsw, migrations);
    std::printf("checksum %lld\n", static_cast<long long>(check));
    std::fclose(log);
    std::free(table);
}
