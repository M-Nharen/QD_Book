// Checks the numeric answers of the Chapter 12 practice problems (Appendix A).
#include <emmintrin.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); \
                                  ++failures; } } while (0)

// Set-associative cache with LRU replacement, for P12.8 and P12.9.
struct Cache {
    std::size_t line, sets, ways;
    std::vector<std::vector<std::uint64_t>> tag;          // per set, most recent first
    long hits = 0, misses = 0;
    Cache(std::size_t l, std::size_t s, std::size_t w)
        : line(l), sets(s), ways(w), tag(s) {}
    void access(std::uint64_t addr) {
        const std::uint64_t blk = addr / line;
        auto& t = tag[blk % sets];
        auto it = std::find(t.begin(), t.end(), blk);
        if (it != t.end()) { ++hits; t.erase(it); }
        else { ++misses; if (t.size() == ways) t.pop_back(); }
        t.insert(t.begin(), blk);
    }
};

// MESI for one line and two cores, for P12.7.
enum St { I, S, E, M };
struct Mesi {
    St st[2] = {I, I};
    int invalidations = 0;
    void read(int c) {
        const int o = 1 - c;
        if (st[c] != I) return;
        if (st[o] == I) st[c] = E;
        else { st[o] = S; st[c] = S; }                 // M/E/S owner downgrades to S
    }
    void write(int c) {
        const int o = 1 - c;
        if (st[o] != I) { st[o] = I; ++invalidations; }
        st[c] = M;
    }
};

// P12.12: index of first byte equal to t, SSE2.
std::size_t find_byte_sse2(const std::uint8_t* a, std::size_t n, std::uint8_t t) {
    const __m128i tv = _mm_set1_epi8(char(t));
    std::size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        const __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
        const unsigned mask = unsigned(_mm_movemask_epi8(_mm_cmpeq_epi8(x, tv)));
        if (mask) return i + std::size_t(std::countr_zero(mask));
    }
    for (; i < n; ++i)
        if (a[i] == t) return i;
    return n;
}

// P12.10: Eytzinger layout and search (same algorithm as Listing 12.8, no prefetch).
void eyt_build(const std::vector<int>& s, std::vector<int>& b, std::size_t& i,
               std::size_t k) {
    if (k >= b.size()) return;
    eyt_build(s, b, i, 2 * k);
    b[k] = s[i++];
    eyt_build(s, b, i, 2 * k + 1);
}

// P12.6: the loop of Listing 11.5, with the side handled by a mask instead of a branch.
[[gnu::noipa]] long long notional_branchy(const unsigned char* p, std::size_t n) {
    long long t = 0;
    for (std::size_t k = 0; k < n; ++k, p += 22) {
        std::int32_t px, q;
        std::memcpy(&px, p + 10, 4);
        std::memcpy(&q, p + 14, 4);
        const long long v = (long long)px * q;
        t += (p[1] == 'B') ? v : -v;
    }
    return t;
}
[[gnu::noipa]] long long notional_mask(const unsigned char* p, std::size_t n) {
    long long t = 0;
    for (std::size_t k = 0; k < n; ++k, p += 22) {
        std::int32_t px, q;
        std::memcpy(&px, p + 10, 4);
        std::memcpy(&q, p + 14, 4);
        const long long v = (long long)px * q;
        const long long m = -static_cast<long long>(p[1] != 'B');   // 0 or all ones
        t += (v ^ m) - m;                                          // v or -v
    }
    return t;
}

int main() {
    // P12.1
    const std::uint64_t addr = 0x7ffd3a2c1e58ULL;
    CHECK((addr & 63) == 24);
    CHECK(((addr >> 6) & 63) == 57);
    // P12.2
    int straddle = 0;
    for (int i = 0; i < 1000; ++i) straddle += (40 * i / 64) != ((40 * i + 39) / 64);
    std::printf("P12.2: %d of 1000 40-byte elements straddle a line boundary\n", straddle);
    CHECK(straddle == 500);
    // P12.3
    CHECK(64 * 4096 == 256 * 1024 && 1536 * 4096 == 6 * 1024 * 1024);
    CHECK(32ULL * (2u << 20) == 64ULL << 20);
    // P12.4
    const double bw = 12 * 64 / 90.0;                     // bytes per ns = GB/s
    std::printf("P12.4: %.2f GB/s per core; lines in flight for 20 GB/s: %.1f\n", bw,
                20.0 * 90.0 / 64.0);
    CHECK(bw > 8.53 && bw < 8.54);
    // P12.7
    Mesi m;
    std::string trace;
    const std::pair<int, char> ops[] = {{0, 'r'}, {1, 'r'}, {0, 'w'}, {1, 'r'}, {1, 'w'},
                                        {0, 'r'}};
    for (auto [core, op] : ops) {
        op == 'r' ? m.read(core) : m.write(core);
        trace += std::string(1, "ISEM"[m.st[0]]) + std::string(1, "ISEM"[m.st[1]]) + " ";
    }
    std::printf("P12.7: (A,B) states: %s invalidations: %d\n", trace.c_str(),
                m.invalidations);
    CHECK(trace == "EI SS MI SS IM SS " && m.invalidations == 2);
    // P12.8: 4 sets x 2 ways, 16-byte lines
    Cache c8(16, 4, 2);
    for (std::uint64_t a : {0, 64, 0, 128, 64, 16, 80, 16, 0, 192}) c8.access(a);
    std::printf("P12.8: %ld hits, %ld misses\n", c8.hits, c8.misses);
    CHECK(c8.hits == 2 && c8.misses == 8);
    // P12.9: 32 KiB, 8-way, 64-byte lines; 256 rows, 8 columns walked top to bottom
    for (std::uint64_t stride : {8192ULL, 8256ULL}) {
        Cache l1(64, 64, 8);
        for (int col = 0; col < 8; ++col)
            for (int row = 0; row < 256; ++row) l1.access(row * stride + col * 8);
        std::printf("P12.9: row stride %llu bytes -> %ld misses of %ld accesses\n",
                    (unsigned long long)stride, l1.misses, l1.hits + l1.misses);
        CHECK(l1.misses == (stride == 8192 ? 2048 : 256));
    }
    // P12.10
    std::vector<int> sorted;
    for (int v = 10; v <= 150; v += 10) sorted.push_back(v);
    std::vector<int> b(16);
    std::size_t idx = 0;
    eyt_build(sorted, b, idx, 1);
    const std::vector<int> expect = {0, 80, 40, 120, 20, 60, 100, 140, 10, 30, 50, 70, 90,
                                     110, 130, 150};
    CHECK(b == expect);
    std::size_t k = 1;
    std::string path;
    while (k <= 15) { path += std::to_string(k) + " "; k = 2 * k + (b[k] < 65); }
    k >>= std::countr_zero(~k) + 1;
    std::printf("P12.10: visited %s-> answer b[%zu] = %d\n", path.c_str(), k, b[k]);
    CHECK(k == 11 && b[k] == 70 && path == "1 2 5 11 ");
    // P12.12
    std::mt19937 rng(3);
    for (int trial = 0; trial < 2000; ++trial) {
        std::vector<std::uint8_t> v(rng() % 200);
        for (auto& x : v) x = std::uint8_t(rng() % 64);
        const std::uint8_t t = std::uint8_t(rng() % 64);
        const std::size_t want = std::size_t(std::find(v.begin(), v.end(), t) - v.begin());
        CHECK(find_byte_sse2(v.data(), v.size(), t) == want);
    }
    // P12.6: same data generator as Listings 11.4/11.5
    const std::size_t N = 1 << 20;
    std::vector<unsigned char> buf(22 * N);
    std::uint64_t s = 12345;
    for (std::size_t i = 0; i < N; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        unsigned char* p = buf.data() + 22 * i;
        const std::int32_t px = 9000 + int((s >> 40) % 2000), q = 1 + int((s >> 20) % 500);
        p[0] = 'A';
        p[1] = (s >> 63) ? 'B' : 'S';
        std::memcpy(p + 10, &px, 4);
        std::memcpy(p + 14, &q, 4);
    }
    double tb = 1e300, tm = 1e300;
    long long rb = 0, rm = 0;
    for (int t = 0; t < 5; ++t) {
        auto t0 = std::chrono::steady_clock::now();
        rb = notional_branchy(buf.data(), N);
        auto t1 = std::chrono::steady_clock::now();
        rm = notional_mask(buf.data(), N);
        auto t2 = std::chrono::steady_clock::now();
        tb = std::min(tb, std::chrono::duration<double, std::nano>(t1 - t0).count() / N);
        tm = std::min(tm, std::chrono::duration<double, std::nano>(t2 - t1).count() / N);
    }
    std::printf("P12.6: branchy %.2f ns/msg, mask %.2f ns/msg (checksums %lld, %lld)\n", tb,
                tm, rb, rm);
    CHECK(rb == rm && rb * 50 == -80498620250LL);     // matches Listings 11.4 and 11.5
    std::printf("%s (%d failures)\n", failures ? "SOME CHECKS FAILED" : "all checks passed",
                failures);
    return failures != 0;
}
