// TLB reach (Section 12.4). The same number of cache lines is visited in random order,
// either packed together (64 lines per 4 KiB page) or spread one line per page, so the
// data footprint is identical and only the number of distinct pages changes. The spread
// layout is then repeated on memory backed by 2 MiB pages (Linux madvise; see 13.3).
#include <sys/mman.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

constexpr std::size_t kLine = 64, kPage = 4096, kHuge = 2u << 20;

struct Region {                                     // 2 MiB-aligned anonymous mapping
    void* map = nullptr;
    std::size_t map_len = 0;
    unsigned char* base = nullptr;
    Region(std::size_t bytes, bool huge) {
        const std::size_t len = (bytes + kHuge - 1) / kHuge * kHuge;
        map_len = len + kHuge;                     // slack so we can align to 2 MiB
        map = mmap(nullptr, map_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
        if (map == MAP_FAILED) { std::perror("mmap"); std::exit(1); }
        auto a = (reinterpret_cast<std::uintptr_t>(map) + kHuge - 1) & ~(kHuge - 1);
        base = reinterpret_cast<unsigned char*>(a);
        madvise(base, len, huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
        std::memset(base, 0, len);                 // fault the pages in now
    }
    ~Region() { munmap(map, map_len); }
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
};

long anon_huge_kib() {                              // from /proc/self/smaps_rollup
    std::ifstream f("/proc/self/smaps_rollup");
    std::string key;
    long v = 0;
    while (f >> key) {
        if (key == "AnonHugePages:") { f >> v; return v; }
    }
    return -1;
}

// Link the lines at the given byte offsets into one random cycle; each line stores the
// offset of the next, so every step is exactly one dependent load.
double chase_ns(unsigned char* base, const std::vector<std::uint32_t>& off,
                std::mt19937& rng) {
    std::vector<std::uint32_t> order(off);
    std::shuffle(order.begin() + 1, order.end(), rng);
    for (std::size_t k = 0; k < order.size(); ++k)
        std::memcpy(base + order[k], &order[(k + 1) % order.size()], 4);
    std::uint32_t o = order[0];
    for (std::size_t s = 0; s < 2 * off.size(); ++s) std::memcpy(&o, base + o, 4);
    const std::size_t steps = 2'000'000;
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t s = 0; s < steps; ++s) std::memcpy(&o, base + o, 4);
    auto t1 = std::chrono::steady_clock::now();
    if (o == 1) std::puts("");                     // keep the chain observable
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(steps);
}

int main() {
    std::mt19937 rng(7);
    long huge_seen = 0;
    std::printf("%8s %10s %12s %14s %14s\n", "lines", "data KiB", "packed ns",
                "1/page 4K ns", "1/page 2M ns");
    for (std::size_t n : {16u, 64u, 256u, 1024u, 4096u, 16384u}) {
        std::vector<std::uint32_t> packed(n), spread(n);
        for (std::size_t k = 0; k < n; ++k) {
            packed[k] = std::uint32_t(k * kLine);
            // One line per page; the line's position inside the page varies so that the
            // lines spread over all cache sets (see 12.2) with 4 KiB and with 2 MiB pages.
            spread[k] = std::uint32_t(k * kPage + ((k + k / 64) % 64) * kLine);
        }
        Region small(n * kLine, false), big4k(n * kPage, false), big2m(n * kPage, true);
        const double a = chase_ns(small.base, packed, rng);
        const double b = chase_ns(big4k.base, spread, rng);
        const double c = chase_ns(big2m.base, spread, rng);
        huge_seen = std::max(huge_seen, anon_huge_kib());
        std::printf("%8zu %10zu %12.2f %14.2f %14.2f\n", n, n * kLine / 1024, a, b,
                    c);
    }
    std::printf("largest AnonHugePages seen: %ld KiB\n", huge_seen);
}
