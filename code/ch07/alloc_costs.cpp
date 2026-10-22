// The nanosecond penalty of allocation (Section 7.9): average cost of an
// allocate+free pair on different paths through glibc malloc, and the alternatives.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <malloc.h>
#include <memory>
#include <new>

inline void escape(void* p) { asm volatile("" : : "r"(p) : "memory"); }

template <class F>
void bench(const char* name, int pairs_per_call, F f, int total_pairs = 2'000'000) {
    const int calls = total_pairs / pairs_per_call;
    for (int i = 0; i < calls / 10; ++i) f();      // warm-up
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < calls; ++i) f();
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("  %-52s %9.1f ns\n", name,
                std::chrono::duration<double, std::nano>(t1 - t0).count() /
                    (double(calls) * pairs_per_call));
}

struct alignas(64) Obj { char b[64]; };

int main() {
    mallopt(M_MMAP_THRESHOLD, 128 * 1024);         // fix the threshold (no dynamic raise)
    mallopt(M_TRIM_THRESHOLD, 128 * 1024);
    std::printf("Average cost per allocate+free pair (single thread):\n");
    bench("stack: alloca-free local array (baseline)", 1, [] {
        char buf[64];
        escape(buf);
    });
    bench("malloc(64)+free, immediately (tcache hit)", 1, [] {
        void* p = std::malloc(64);
        escape(p);
        std::free(p);
    });
    bench("new/delete of a 64-byte object", 1, [] {
        auto* p = new char[64];
        escape(p);
        delete[] p;
    });
    bench("aligned new/delete, alignas(64) object", 1, [] {
        auto* p = new Obj;
        escape(p);
        delete p;
    });
    bench("64 x malloc(64), then 64 x free (overflows tcache)", 64, [] {
        void* p[64];
        for (auto& x : p) { x = std::malloc(64); escape(x); }
        for (auto& x : p) std::free(x);
    });
    bench("malloc(4 KiB)+free", 1, [] {
        void* p = std::malloc(4096);
        escape(p);
        std::free(p);
    });
    bench("malloc(1 MiB)+free: mmap+munmap, untouched", 1, [] {
        void* p = std::malloc(1 << 20);
        escape(p);
        std::free(p);
    }, 20'000);
    bench("malloc(1 MiB)+free, writing 1 byte per 4 KiB page", 1, [] {
        char* p = static_cast<char*>(std::malloc(1 << 20));
        for (int i = 0; i < (1 << 20); i += 4096) p[i] = 1;
        escape(p);
        std::free(p);
    }, 2'000);
    bench("make_unique<long>", 1, [] {
        auto p = std::make_unique<long>(1);
        escape(p.get());
    });
    bench("make_shared<long>", 1, [] {
        auto p = std::make_shared<long>(1);
        escape(p.get());
    });

    // A free-list pool, inline: the alternative of Listing 7.9.
    static Obj slab[1024];
    static Obj* free_list[1024];
    static int top = 0;
    for (auto& o : slab) free_list[top++] = &o;
    bench("free-list pool: pop + push", 1, [] {
        Obj* p = free_list[--top];
        escape(p);
        free_list[top++] = p;
    });
    static char arena[1 << 16];
    static std::size_t used = 0;
    bench("bump arena: take 64 bytes (+reset every 1000)", 1, [] {
        if (used + 64 > 64000) used = 0;
        char* p = arena + used;
        used += 64;
        escape(p);
    });
}
