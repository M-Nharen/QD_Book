// Checks the numeric answers to the Chapter 7 practice problems (Appendix A).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <malloc.h>
#include <memory>
#include <new>
#include <string>
#include <vector>

int failures = 0;
void check(const char* what, long got, long want) {
    std::printf("  %-48s %8ld  %s\n", what, got, got == want ? "ok" : "MISMATCH");
    if (got != want) ++failures;
}

long g_news = 0;
std::size_t g_last = 0;
void* acquire(std::size_t n, std::size_t al) {
    ++g_news;
    g_last = n;
    void* p = nullptr;
    if (posix_memalign(&p, al < 16 ? 16 : al, n ? n : 1) != 0) throw std::bad_alloc{};
    return p;
}
[[gnu::noinline]] void release(void* p) noexcept { std::free(p); }
void* operator new(std::size_t n) { return acquire(n, 16); }
void* operator new[](std::size_t n) { return acquire(n, 16); }
void* operator new[](std::size_t n, std::align_val_t a) { return acquire(n, size_t(a)); }
void operator delete(void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::align_val_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release(p); }

template <class F> long news(F f) { const long b = g_news; f(); return g_news - b; }
inline void escape(void* p) { asm volatile("" : : "r"(p) : "memory"); }

struct D24 { long a, b, c; ~D24() { a = 0; } };
struct alignas(32) D32 { long a; ~D32() { a = 0; } };

std::size_t place(std::size_t& used, std::size_t n, std::size_t al) {   // P7.5 arena
    const std::size_t off = (used + al - 1) & ~(al - 1);
    used = off + n;
    return off;
}

int main() {
    std::printf("P7.1 usable size = max(32, (n + 8 + 15) & ~15) - 8\n");
    for (std::size_t n : {0, 24, 25, 56, 57, 1000}) {
        const std::size_t chunk = std::max<std::size_t>(32, (n + 23) & ~std::size_t(15));
        const long formula = long(chunk) - 8;
#ifdef __SANITIZE_ADDRESS__
        (void)formula;                             // ASan's allocator has other rules
#else
        void* p = std::malloc(n);
        char label[48];
        std::snprintf(label, sizeof label, "malloc_usable_size(malloc(%zu))", n);
        check(label, long(malloc_usable_size(p)), formula);
        std::free(p);
#endif
    }

    std::printf("P7.2 operator new calls\n");
    check("(a) string of 15 chars", news([] { std::string s(15, 'x'); escape(&s); }), 0);
    check("(b) string of 20 chars", news([] { std::string s(20, 'x'); escape(&s); }), 1);
    check("(c) vector<int> v; v.reserve(0)", news([] {
        std::vector<int> v;
        v.reserve(0);
        escape(&v);
    }), 0);
    check("(d) function capturing 8-byte long", news([] {
        long x = 1;
        std::function<long()> f = [x] { return x; };
        escape(&f);
    }), 0);
    auto sp = std::make_shared<int>(1);            // allocated outside the count
    check("(e) function capturing shared_ptr (16 B)", news([&] {
        std::function<int()> f = [sp] { return *sp; };
        escape(&f);
    }), 1);

    std::printf("P7.3 page faults to touch 1 GiB\n");
    check("4 KiB pages", (1L << 30) / 4096, 262144);
    check("2 MiB pages", (1L << 30) / (2L << 20), 512);
    check("ms at 1.5 us per 4 KiB fault", long(262144 * 1.5 / 1000 + 0.5), 393);

    std::printf("P7.4 bytes requested by new T[5]\n");
    {
        D24* a = new D24[5];
        escape(a);
        check("sizeof 24, non-trivial dtor", long(g_last), 128);
        delete[] a;
        D32* b = new D32[5];
        escape(b);
        check("alignof 32, sizeof 32, non-trivial dtor", long(g_last), 192);
        delete[] b;
    }

    std::printf("P7.5 arena offsets, (size,align) = (1,1)(8,8)(2,2)(64,64)(4,4)(16,16)\n");
    std::size_t used = 0;
    const std::size_t want[] = {0, 8, 16, 64, 128, 144};
    const std::size_t req[][2] = {{1, 1}, {8, 8}, {2, 2}, {64, 64}, {4, 4}, {16, 16}};
    for (int i = 0; i < 6; ++i) {
        char label[48];
        std::snprintf(label, sizeof label, "offset of allocation %d", i + 1);
        check(label, long(place(used, req[i][0], req[i][1])), long(want[i]));
    }
    check("bytes used at the end", long(used), 160);
    used = 0;                                      // most-aligned first: no padding
    for (int i : {3, 5, 1, 4, 2, 0}) place(used, req[i][0], req[i][1]);
    check("bytes used, sorted by alignment", long(used), 95);
    std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
