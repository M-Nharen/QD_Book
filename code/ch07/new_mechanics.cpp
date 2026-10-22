// What a new-expression asks of operator new: sizes, array cookies, alignment,
// placement new, sized delete, and cleanup when a constructor throws (Section 7.3).
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>

bool g_log = false;                                // log only inside the demo region

void* operator new(std::size_t n) {
    if (g_log) std::printf("    operator new(%zu)\n", n);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t n) {
    if (g_log) std::printf("    operator new[](%zu)\n", n);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t n, std::align_val_t al) {
    if (g_log) std::printf("    operator new(%zu, align %zu)\n", n, std::size_t(al));
    void* p = nullptr;
    if (posix_memalign(&p, std::size_t(al), n ? n : 1) != 0) throw std::bad_alloc{};
    return p;
}
// Out of line so GCC's -Wmismatched-new-delete does not pair the inlined free() with
// operator new; releasing posix_memalign memory with free() is correct.
[[gnu::noinline]] void release(void* p) noexcept { std::free(p); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept {
    if (g_log) std::printf("    operator delete[](ptr)   <- unsized: no cookie\n");
    release(p);
}
void operator delete(void* p, std::size_t n) noexcept {
    if (g_log) std::printf("    operator delete(ptr, %zu)   <- sized delete\n", n);
    release(p);
}
void operator delete[](void* p, std::size_t n) noexcept {
    if (g_log) std::printf("    operator delete[](ptr, %zu)\n", n);
    release(p);
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept {
    if (g_log) std::printf("    operator delete(ptr, %zu, align %zu)\n", n, size_t(al));
    release(p);
}
void operator delete(void* p, std::align_val_t) noexcept { release(p); }

// Make a pointer "escape" so the optimizer cannot remove the allocation.
inline void escape(void* p) { asm volatile("" : : "r"(p) : "memory"); }

struct Trivial { long v; };                        // trivial destructor
struct WithDtor { long v; ~WithDtor() { v = 0; } }; // non-trivial destructor
struct alignas(64) Aligned64 { char bytes[64]; };   // one cache line (Section 12.2)
struct Throws {
    Throws() { throw std::runtime_error("ctor failed"); }
    long v = 0;
};

int main() {
    g_log = true;
    std::printf("new long:\n");
    long* a = new long(7);                         // no escape: GCC -O2 may delete
    delete a;                                      // the pair (allocation elision)
    std::printf("new Trivial[10] (sizeof = %zu):\n", sizeof(Trivial));
    Trivial* t = new Trivial[10];
    escape(t);
    delete[] t;
    std::printf("new WithDtor[10] (sizeof = %zu): 8 extra bytes hold the count\n",
                sizeof(WithDtor));
    WithDtor* w = new WithDtor[10];
    escape(w);
    delete[] w;
    std::printf("new Aligned64 (alignof = %zu):\n", alignof(Aligned64));
    Aligned64* x = new Aligned64{};
    std::printf("    address %% 64 = %zu\n", reinterpret_cast<std::uintptr_t>(x) % 64);
    delete x;
    std::printf("placement new / construct_at into a buffer: no operator new call\n");
    alignas(WithDtor) unsigned char buf[sizeof(WithDtor)];
    WithDtor* p = new (buf) WithDtor{5};
    p->~WithDtor();                                // we constructed it, we destroy it
    WithDtor* q = std::construct_at(reinterpret_cast<WithDtor*>(buf), WithDtor{6});
    std::destroy_at(q);
    std::printf("constructor throws: the matching operator delete runs automatically\n");
    try {
        Throws* bad = new Throws;
        delete bad;                                // never reached
    } catch (const std::exception& e) {
        std::printf("    caught: %s\n", e.what());
    }
    g_log = false;
}
