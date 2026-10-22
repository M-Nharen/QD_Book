// Cost of misaligned 8-byte loads (Section 12.9): inside one cache line, split across two
// lines, and split across two 4 KiB pages. All data stays in L1. Loads use memcpy, so the
// program has no alignment UB (11.3); the compiler emits plain mov instructions.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

[[gnu::noipa]] std::uint64_t sum_loads(const unsigned char* base,
                                       const std::vector<std::uint32_t>& off, int reps) {
    std::uint64_t s = 0;
    for (int r = 0; r < reps; ++r)
        for (std::uint32_t o : off) {
            std::uint64_t v;
            std::memcpy(&v, base + o, 8);               // one 8-byte load
            s += v;
        }
    return s;
}

int main() {
    constexpr std::size_t kPage = 4096, kPages = 8;
    auto* buf = static_cast<unsigned char*>(std::aligned_alloc(kPage, kPages * kPage));
    std::memset(buf, 1, kPages * kPage);
    struct Case { const char* name; std::vector<std::uint32_t> off; };
    std::vector<Case> cases(4);
    cases[0].name = "aligned (offset 0 in line)";
    cases[1].name = "misaligned, same line (3)";
    cases[2].name = "split across lines (60)";
    cases[3].name = "split across pages (4092)";
    for (std::uint32_t i = 0; i < 64; ++i) {           // 64 lines spread over the pages
        const std::uint32_t line = i * 7 * 64;          // every 7th line: varied sets
        cases[0].off.push_back(line);
        cases[1].off.push_back(line + 3);
        cases[2].off.push_back(line + 60);
        cases[3].off.push_back(std::uint32_t((i % (kPages - 1)) * kPage + 4092));
    }
    const int reps = 20000;
    std::uint64_t sink = 0;
    std::printf("%-28s %s\n", "load address", "ns per load");
    for (const Case& c : cases) {
        double best = 1e300;
        for (int t = 0; t < 7; ++t) {
            auto t0 = std::chrono::steady_clock::now();
            sink += sum_loads(buf, c.off, reps);
            auto t1 = std::chrono::steady_clock::now();
            const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
            best = std::min(best, ns);
        }
        std::printf("%-28s %.3f\n", c.name, best / (double(reps) * double(c.off.size())));
    }
    std::printf("(checksum %llu)\n", (unsigned long long)sink);
    std::free(buf);
}
