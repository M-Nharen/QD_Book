// Code showdown, tuned version (Section 13.9). Same table, same events, same per-event
// timing as Listing 13.7; only the process setup and the logging differ.
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

struct Record {                                    // 32 bytes: two per cache line
    std::int64_t position, notional, last_px;
    std::uint64_t updates;
};
constexpr std::uint32_t kRecords = 1u << 22, kEvents = 1u << 18;
constexpr std::size_t kHuge = 2u << 20;
constexpr std::size_t kTableBytes = std::size_t(kRecords) * sizeof(Record);

static Record* map_table() {                       // 2 MiB-aligned, THP-backed, pre-faulted
    void* m = mmap(nullptr, kTableBytes + kHuge, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) return nullptr;
    auto a = (reinterpret_cast<std::uintptr_t>(m) + kHuge - 1) & ~(kHuge - 1);
    void* t = reinterpret_cast<void*>(a);
    madvise(t, kTableBytes, MADV_HUGEPAGE);        // before the first touch
    std::memset(t, 0, kTableBytes);                // write every page now, not in the loop
    return static_cast<Record*>(t);
}

static long anon_huge_kb() {                       // whole-process AnonHugePages
    std::ifstream f("/proc/self/smaps_rollup");
    for (std::string l; std::getline(f, l);)
        if (l.rfind("AnonHugePages:", 0) == 0) return std::stol(l.substr(14));
    return -1;
}

struct LogEntry { std::uint32_t event; std::int64_t position; };

int main() {
    // 1. Allocate and touch everything the hot loop will use.
    Record* table = map_table();
    if (!table) return 1;
    std::vector<std::uint32_t> lat_ns(kEvents);    // value-initialized: pages touched
    std::vector<LogEntry> log;
    log.reserve(kEvents / 1024 + 1);               // in-memory log, no syscalls in loop
    // 2. Lock it in RAM. After the touching, so the THP request above is honoured.
    const bool locked = mlockall(MCL_CURRENT | MCL_FUTURE) == 0;
    // 3. Pin this thread to one CPU (the last one; CPU 0 is left for housekeeping).
    const int cpu = static_cast<int>(std::thread::hardware_concurrency()) - 1;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    const bool pinned = pthread_setaffinity_np(pthread_self(), sizeof set, &set) == 0;

    // 4. Warm up every call the loop makes: the first clock read faults in the vDSO's
    //    data page, which mlockall does not populate.
    (void)std::chrono::steady_clock::now();
    int where = sched_getcpu(), migrations = 0;
    rusage r0{}, r1{};
    getrusage(RUSAGE_SELF, &r0);
    std::uint64_t x = 88172645463325252ull;
    for (std::uint32_t i = 0; i < kEvents; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        Record& r = table[x % kRecords];
        const std::int64_t qty = std::int64_t(x >> 40 & 255) - 128, px = 10000 + (x & 1023);
        r.position += qty;
        r.notional += qty * px;
        r.last_px = px;
        ++r.updates;
        if ((i & 1023) == 0) log.push_back({i, r.position});  // no allocation: reserved
        const auto t1 = std::chrono::steady_clock::now();
        lat_ns[i] = static_cast<std::uint32_t>((t1 - t0).count());
        if (const int c = sched_getcpu(); c != where) { ++migrations; where = c; }
    }
    getrusage(RUSAGE_SELF, &r1);

    if (FILE* f = std::fopen("/dev/null", "w")) {  // drain the log off the hot path
        for (const auto& e : log)
            std::fprintf(f, "event %u pos %lld\n", e.event,
                         static_cast<long long>(e.position));
        std::fclose(f);
    }
    std::int64_t check = 0;
    for (std::uint32_t k = 0; k < kRecords; ++k) check += table[k].position;
    std::sort(lat_ns.begin(), lat_ns.end());
    auto pct = [&](double p) { return lat_ns[std::size_t(p * (kEvents - 1))]; };
    std::printf("tuned: p50 %u  p90 %u  p99 %u  p99.9 %u  max %u ns\n", pct(0.5),
                pct(0.9), pct(0.99), pct(0.999), lat_ns.back());
    std::printf("during the loop: %ld minor faults, %ld voluntary / %ld involuntary "
                "switches, %d migrations\n", r1.ru_minflt - r0.ru_minflt,
                r1.ru_nvcsw - r0.ru_nvcsw, r1.ru_nivcsw - r0.ru_nivcsw, migrations);
    std::printf("mlockall %s, pinned to CPU %d: %s, AnonHugePages %ld MiB\n",
                locked ? "ok" : "failed (RLIMIT_MEMLOCK?)", cpu, pinned ? "yes" : "no",
                anon_huge_kb() >> 10);
    std::printf("checksum %lld\n", static_cast<long long>(check));
}
