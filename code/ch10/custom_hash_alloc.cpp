// Transparent comparators, composite keys, and std::pmr with node containers (10.6).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory_resource>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>

static long g_allocs = 0;                     // global operator new calls
[[gnu::noinline]] void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// Composite key: pack small fields into one integer instead of combining two hashes.
struct LevelKey { std::uint32_t instrument; std::int32_t price_ticks; };
constexpr std::uint64_t pack(LevelKey k) {
    return (std::uint64_t{k.instrument} << 32) | static_cast<std::uint32_t>(k.price_ticks);
}
static_assert(pack({1, -1}) == 0x0000'0001'FFFF'FFFFULL);   // injective: no collisions

int main() {
    // 1. std::less<> is "transparent": find() accepts anything comparable with the key,
    //    so looking up a string_view does not build a temporary std::string.
    std::map<std::string, int, std::less<>> symbols;
    symbols.emplace("XNSE:RELIANCE-FUTURES-2026-12", 1);     // 29 chars: heap-allocated
    const std::string_view sv = "XNSE:RELIANCE-FUTURES-2026-12";
    long a0 = g_allocs;
    const bool found = symbols.find(sv) != symbols.end();
    std::printf("map<string,int,less<>>::find(string_view): found=%d allocations=%ld\n",
                found, g_allocs - a0);

    // 2. A comparator chooses the order. Bids are best when highest: std::greater<>.
    std::map<std::int64_t, std::int64_t, std::greater<>> bids{{100, 5}, {102, 1}, {101, 7}};
    std::printf("best bid = %lld (first element under std::greater)\n",
                static_cast<long long>(bids.begin()->first));

    // 3. Packed composite key in an ordinary integer-keyed map.
    std::unordered_map<std::uint64_t, std::int64_t> qty_at_level;
    qty_at_level[pack({7, 1'000'500})] += 300;
    qty_at_level[pack({7, 1'000'500})] += 200;
    std::printf("qty at (7, 1000500) = %lld\n",
                static_cast<long long>(qty_at_level.at(pack({7, 1'000'500}))));

    // 4. std::pmr: nodes come from a buffer we own instead of the global heap. A
    //    monotonic resource never frees individual blocks; memory returns when the
    //    resource is destroyed. null_memory_resource() as upstream makes running out
    //    of buffer throw instead of silently falling back to malloc.
    alignas(std::max_align_t) static std::array<std::byte, 1 << 20> buffer;
    std::pmr::monotonic_buffer_resource arena(buffer.data(), buffer.size(),
                                              std::pmr::null_memory_resource());
    a0 = g_allocs;
    {
        std::pmr::unordered_map<std::uint64_t, std::int64_t> orders(&arena);
        orders.reserve(4'096);
        for (std::uint64_t id = 0; id < 4'000; ++id) orders.emplace(id, 100);
        for (std::uint64_t id = 0; id < 4'000; id += 2) orders.erase(id);
        std::printf("pmr unordered_map: size=%zu, global allocations=%ld\n",
                    orders.size(), g_allocs - a0);
    }
}
