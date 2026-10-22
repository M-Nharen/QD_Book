// book: timeout=180
// Smaller object-model costs: RTTI casts and loads that straddle a cache line (6.9).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <typeinfo>
#include <vector>

struct Base { virtual ~Base() = default; long v = 1; };
struct Mid : Base {};
struct Leaf final : Mid {};
struct Other final : Base {};

volatile std::uint64_t sink;

template <class F>
void bench(const char* name, long n, F f) {
    double best = 1e30;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        sink = f();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
        best = ns < best ? ns : best;
    }
    std::printf("  %-46s %6.2f ns\n", name, best);
}

// Pointer chase through 8-byte values stored at a chosen offset within each 64-byte
// line. Each load's address depends on the previous load, so we measure load latency.
std::uint64_t chase(const unsigned char* buf, std::size_t start, long steps) {
    std::uint64_t pos = start;
    for (long i = 0; i < steps; ++i) {
        std::uint64_t next;
        std::memcpy(&next, buf + pos, sizeof next);  // compiles to one 8-byte mov
        pos = next;
    }
    return pos;
}

int main() {
    const long n = 20'000'000;
    Leaf leaf;
    Other other;
    Base* volatile pl = &leaf;                   // volatile: hide the dynamic type
    Base* volatile po = &other;
    std::printf("Casts (ns per cast):\n");
    bench("static_cast<Leaf*> (no check)", n, [&] {
        long s = 0;
        for (long i = 0; i < n; ++i) s += static_cast<Leaf*>(pl)->v;
        return s;
    });
    bench("typeid(*p) == typeid(Leaf)", n, [&] {
        long s = 0;
        for (long i = 0; i < n; ++i) s += typeid(*pl) == typeid(Leaf);
        return s;
    });
    bench("dynamic_cast<Leaf*>, succeeds (2 levels)", n, [&] {
        long s = 0;
        for (long i = 0; i < n; ++i) s += dynamic_cast<Leaf*>(pl) != nullptr;
        return s;
    });
    bench("dynamic_cast<Leaf*>, fails", n, [&] {
        long s = 0;
        for (long i = 0; i < n; ++i) s += dynamic_cast<Leaf*>(po) != nullptr;
        return s;
    });

    std::printf("Dependent 8-byte loads within L1 (ns per load):\n");
    const int lines = 64;                        // 4 KB: comfortably inside L1d
    std::vector<unsigned char> buf(lines * 64 + 64);
    for (std::size_t off : {std::size_t{0}, std::size_t{60}}) {
        for (int l = 0; l < lines; ++l) {        // line l points to line (l + 17) % 64
            const std::uint64_t next = std::uint64_t((l + 17) % lines) * 64 + off;
            std::memcpy(buf.data() + l * 64 + off, &next, sizeof next);
        }
        bench(off == 0 ? "aligned, within one line" : "misaligned, straddling two lines", n,
              [&] { return chase(buf.data(), off, n); });
    }
}
