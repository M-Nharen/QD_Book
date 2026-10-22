// Object layout: alignment, padding, reordering, cache lines, packed wire formats (6.1).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <type_traits>

struct S { char a; int b; char c; };             // diagnostic question D4
struct SReordered { int b; char a; char c; };

struct OrderNaive {                              // fields in "logical" order
    bool is_buy;
    std::int64_t id;
    std::int32_t qty;
    bool is_ioc;
    std::int64_t price_ticks;
    std::uint16_t venue;
};
struct OrderSorted {                             // same fields, largest alignment first
    std::int64_t id;
    std::int64_t price_ticks;
    std::int32_t qty;
    std::uint16_t venue;
    bool is_buy;
    bool is_ioc;
};

struct alignas(64) HotQuote {                    // starts every object on a cache line
    std::int64_t bid_px, ask_px;
    std::int32_t bid_qty, ask_qty;
};

#pragma pack(push, 1)                            // wire format: no padding allowed
struct WireAddOrder {
    char msg_type;                               // offset 0
    std::uint64_t order_id;                      // offset 1: misaligned on purpose
    std::uint32_t price;                         // offset 9
    std::uint32_t qty;                           // offset 13
    char side;                                   // offset 17
};
#pragma pack(pop)
static_assert(sizeof(WireAddOrder) == 18 && offsetof(WireAddOrder, price) == 9);

template <class T> void show(const char* name) {
    std::printf("  %-13s sizeof %3zu  alignof %2zu  padding-free: %s\n", name, sizeof(T),
                alignof(T), std::has_unique_object_representations_v<T> ? "yes" : "no");
}

int main() {
    std::printf("Sizes:\n");
    show<S>("S");
    show<SReordered>("SReordered");
    show<OrderNaive>("OrderNaive");
    show<OrderSorted>("OrderSorted");
    show<HotQuote>("HotQuote");
    show<WireAddOrder>("WireAddOrder");
    std::printf("OrderNaive offsets: id %zu, qty %zu, is_ioc %zu, price %zu, venue %zu\n",
                offsetof(OrderNaive, id), offsetof(OrderNaive, qty),
                offsetof(OrderNaive, is_ioc), offsetof(OrderNaive, price_ticks),
                offsetof(OrderNaive, venue));

    // An array starting on a 64-byte boundary: which elements straddle two lines?
    for (std::size_t sz : {16, 24, 32, 40, 48}) {
        int split = 0;
        for (std::size_t k = 0; k < 64; ++k) {
            const std::size_t start = k * sz, end = start + sz - 1;
            split += start / 64 != end / 64;
        }
        std::printf("element size %2zu: %2d of 64 elements span two lines\n", sz, split);
    }
}
