// book: timeout=240
// Code showdown, optimized: events as a std::variant in a reused contiguous buffer (6.8).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <variant>
#include <vector>

static long g_allocs = 0;                        // count heap allocations (Listing 5.5)
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct RawMsg {                                  // what the feed decoder hands us
    std::uint8_t type;                           // 0 add, 1 cancel, 2 trade
    std::uint8_t side;
    std::int32_t qty;
    std::int64_t id, px;
};

struct Book {                                    // aggregated depth plus live orders
    static constexpr std::int64_t lo = 10'000, levels = 1'000;
    std::vector<std::int64_t> depth = std::vector<std::int64_t>(levels, 0);
    struct Slot { std::int64_t px; std::int32_t qty; };
    std::vector<Slot> orders;                    // indexed by order id
    std::int64_t traded = 0;
};

struct AddOrder {                                // plain values: no vptr, no heap
    std::int64_t id, px;
    std::int32_t qty;
    void apply(Book& b) const {
        b.orders[id] = {px, qty};
        b.depth[px - Book::lo] += qty;
    }
};
struct CancelOrder {
    std::int64_t id;
    void apply(Book& b) const {
        Book::Slot& s = b.orders[id];
        b.depth[s.px - Book::lo] -= s.qty;
        s.qty = 0;
    }
};
struct Trade {
    std::int64_t px;
    std::int32_t qty;
    void apply(Book& b) const { b.traded += qty * px; }
};
using Event = std::variant<AddOrder, CancelOrder, Trade>;
static_assert(sizeof(Event) == 32);

std::vector<RawMsg> make_stream(long n, long& adds) {    // 50% add, 40% cancel, 10% trade
    std::vector<RawMsg> v;
    v.reserve(n);
    std::uint64_t x = 7;
    adds = 0;
    for (long i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        const int r = static_cast<int>(x % 10);
        const std::int64_t px = 10'000 + static_cast<std::int64_t>((x >> 8) % 1'000);
        const std::int32_t qty = 1 + static_cast<std::int32_t>((x >> 24) % 100);
        if (r < 5 || adds == 0) v.push_back({0, 0, qty, adds++, px});
        else if (r < 9) v.push_back({1, 0, 0, static_cast<long>((x >> 32) % adds), 0});
        else v.push_back({2, 0, qty, 0, px});
    }
    return v;
}

int main() {
    long adds = 0;
    const long n = 2'000'000;
    const std::vector<RawMsg> stream = make_stream(n, adds);
    double best = 1e30;
    long allocs = 0;
    Book book;
    std::vector<Event> batch;                    // lives across batches:
    batch.reserve(n);                            // allocate once, at start-up
    for (int rep = 0; rep < 5; ++rep) {
        book = Book{};
        book.orders.assign(adds, {0, 0});
        const long a0 = g_allocs;
        const auto t0 = std::chrono::steady_clock::now();
        for (const RawMsg& m : stream) {                     // decode in place...
            if (m.type == 0) batch.emplace_back(AddOrder{m.id, m.px, m.qty});
            else if (m.type == 1) batch.emplace_back(CancelOrder{m.id});
            else batch.emplace_back(Trade{m.px, m.qty});
        }
        for (const Event& e : batch)                         // ...then apply
            std::visit([&](const auto& ev) { ev.apply(book); }, e);
        batch.clear();                                       // keeps the capacity
        const auto t1 = std::chrono::steady_clock::now();
        allocs = g_allocs - a0;
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
        best = ns < best ? ns : best;
    }
    std::int64_t resting = 0;
    for (std::int64_t q : book.depth) resting += q;
    std::printf("events %ld, resting qty %lld, traded notional %lld\n", n,
                static_cast<long long>(resting), static_cast<long long>(book.traded));
    std::printf("heap allocations per batch: %ld\n", allocs);
    std::printf("best time: %.1f ns per event\n", best);
}
