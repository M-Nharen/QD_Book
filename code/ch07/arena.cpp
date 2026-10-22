// A bump-pointer (monotonic) arena: allocation is an add and a compare; there is no
// per-object free, only reset() of the whole arena (Section 7.5).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>
#include <utility>

class Arena {
    std::byte* base_;
    std::size_t cap_;
    std::size_t used_ = 0;

public:
    Arena(void* buffer, std::size_t capacity)
        : base_(static_cast<std::byte*>(buffer)), cap_(capacity) {}
    Arena(const Arena&) = delete;                  // two owners of one cursor = bugs
    Arena& operator=(const Arena&) = delete;

    // align must be a power of two. Returns nullptr when full: the caller decides.
    void* allocate(std::size_t n, std::size_t align) noexcept {
        const auto start = reinterpret_cast<std::uintptr_t>(base_);
        const std::uintptr_t cur = start + used_;
        const std::uintptr_t aligned = (cur + align - 1) & ~std::uintptr_t(align - 1);
        const std::size_t end = (aligned - start) + n;
        if (end > cap_) return nullptr;
        used_ = end;
        return reinterpret_cast<void*>(aligned);
    }

    // reset() runs no destructors, so only allow types that need none.
    template <class T, class... Args>
    T* make(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Arena::reset() would skip this type's destructor");
        void* p = allocate(sizeof(T), alignof(T));
        return p ? ::new (p) T(std::forward<Args>(args)...) : nullptr;
    }

    void reset() noexcept { used_ = 0; }
    std::size_t used() const noexcept { return used_; }
};

struct Fill { std::uint64_t order_id; std::int64_t price; std::uint32_t qty; };
struct alignas(64) Line { char bytes[64]; };

int main() {
    alignas(64) static std::byte storage[1024];    // static: no stack, no heap
    Arena arena(storage, sizeof storage);

    std::printf("sizeof(Fill)=%zu alignof(Fill)=%zu\n", sizeof(Fill), alignof(Fill));
    for (int msg = 0; msg < 2; ++msg) {            // one "message" per iteration
        char* c = arena.make<char>('x');           // 1 byte; next object needs padding
        Fill* f1 = arena.make<Fill>(Fill{100 + std::uint64_t(msg), 9950, 10});
        Fill* f2 = arena.make<Fill>(Fill{200 + std::uint64_t(msg), 9951, 5});
        Line* l = arena.make<Line>();
        std::printf("msg %d: offsets char=%td fill1=%td fill2=%td line=%td used=%zu\n",
                    msg, reinterpret_cast<std::byte*>(c) - storage,
                    reinterpret_cast<std::byte*>(f1) - storage,
                    reinterpret_cast<std::byte*>(f2) - storage,
                    reinterpret_cast<std::byte*>(l) - storage, arena.used());
        arena.reset();                             // end of message: free everything
    }
    int made = 0;
    while (arena.make<Fill>(Fill{0, 0, 0})) ++made;
    std::printf("Fills that fit in 1024 bytes: %d (then make() returns nullptr)\n", made);
}
