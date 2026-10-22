// Solution P7.9: a minimal standard-conforming allocator over a bump arena.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <new>
#include <vector>

class Arena {
    std::byte* base_;
    std::size_t cap_, used_ = 0;
public:
    Arena(void* buf, std::size_t cap) : base_(static_cast<std::byte*>(buf)), cap_(cap) {}
    void* allocate(std::size_t n, std::size_t align) {
        const auto start = reinterpret_cast<std::uintptr_t>(base_);
        const std::uintptr_t p = (start + used_ + align - 1) & ~std::uintptr_t(align - 1);
        if (p - start + n > cap_) throw std::bad_alloc{};
        used_ = p - start + n;
        return reinterpret_cast<void*>(p);
    }
    std::size_t used() const { return used_; }
};

template <class T> struct ArenaAllocator {
    using value_type = T;                          // the only required member type
    Arena* arena;
    explicit ArenaAllocator(Arena& a) noexcept : arena(&a) {}
    template <class U>                             // rebinding: map allocates nodes,
    ArenaAllocator(const ArenaAllocator<U>& o) noexcept : arena(o.arena) {}  // not pairs
    T* allocate(std::size_t n) {
        return static_cast<T*>(arena->allocate(n * sizeof(T), alignof(T)));
    }
    void deallocate(T*, std::size_t) noexcept {}   // monotonic: memory reused on reset
    template <class U> bool operator==(const ArenaAllocator<U>& o) const noexcept {
        return arena == o.arena;                   // equal = can free each other's memory
    }
};

int main() {
    alignas(std::max_align_t) static std::byte buf[1 << 16];
    Arena arena(buf, sizeof buf);

    std::vector<int, ArenaAllocator<int>> v{ArenaAllocator<int>(arena)};
    v.reserve(100);
    for (int i = 0; i < 100; ++i) v.push_back(i * i);
    std::printf("vector: arena used %zu bytes (100 ints = 400)\n", arena.used());

    using Alloc = ArenaAllocator<std::pair<const int, long>>;
    std::map<int, long, std::less<int>, Alloc> book{Alloc(arena)};
    for (int px = 100; px < 110; ++px) book[px] = px * 10;
    std::printf("map with 10 levels: arena used %zu bytes in total\n", arena.used());
    long sum = 0;
    for (const auto& [px, q] : book) sum += q;
    std::printf("sum of quantities %ld, v[99] = %d\n", sum, v[99]);
}
