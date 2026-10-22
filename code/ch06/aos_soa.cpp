// book: timeout=240
// Array of structs vs hot/cold split vs struct of arrays for a scan query (6.7).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Order {                                   // one order = one 64-byte cache line
    std::int64_t id, price_ticks, create_ns, update_ns, client_id, strategy_id;
    std::int32_t qty, filled;
    std::uint16_t venue, trader;
    std::uint8_t side, type, tif, flags;
};
static_assert(sizeof(Order) == 64);

struct OrderHot {                                // the fields the scan needs: 16 bytes
    std::int64_t price_ticks;
    std::int32_t qty;
    std::uint8_t side;
};
static_assert(sizeof(OrderHot) == 16);

struct OrderColumns {                            // struct of arrays: one vector per field
    std::vector<std::int64_t> price_ticks;
    std::vector<std::int32_t> qty;
    std::vector<std::uint8_t> side;              // (other columns omitted: never read)
};

constexpr std::uint8_t kBuy = 0;

// Query: total buy quantity at or above a price, as a risk check might compute it.
// The condition becomes an all-ones or all-zeros mask, so there is no branch to mispredict
// (with && and ?:, GCC emits two branches that random data mispredicts; see the text).
inline long take(bool cond, std::int32_t qty) { return qty & -static_cast<long>(cond); }
long scan_aos(const std::vector<Order>& v, std::int64_t px) {
    long sum = 0;
    for (const Order& o : v)
        sum += take((o.side == kBuy) & (o.price_ticks >= px), o.qty);
    return sum;
}
long scan_hot(const std::vector<OrderHot>& v, std::int64_t px) {
    long sum = 0;
    for (const OrderHot& o : v)
        sum += take((o.side == kBuy) & (o.price_ticks >= px), o.qty);
    return sum;
}
long scan_soa(const OrderColumns& c, std::int64_t px) {
    long sum = 0;
    const std::size_t n = c.qty.size();
    for (std::size_t i = 0; i < n; ++i)
        sum += take((c.side[i] == kBuy) & (c.price_ticks[i] >= px), c.qty[i]);
    return sum;
}

template <class F>
void bench(const char* name, double bytes_per_order, long n, F f) {
    double best = 1e30;
    long result = 0;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        result = f();
        const auto t1 = std::chrono::steady_clock::now();
        const double s = std::chrono::duration<double>(t1 - t0).count();
        best = s < best ? s : best;
    }
    std::printf("  %-22s %5.1f B/order  %6.2f ns/order  %5.1f GB/s  (result %ld)\n", name,
                bytes_per_order, best * 1e9 / n, bytes_per_order * n / best / 1e9, result);
}

int main() {
    const long n = 4'000'000;                    // 256 MB as AoS: far beyond any cache
    std::vector<Order> aos(n);
    std::vector<OrderHot> hot(n);
    OrderColumns soa;
    soa.price_ticks.resize(n);
    soa.qty.resize(n);
    soa.side.resize(n);
    std::uint64_t x = 1;
    for (long i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        Order& o = aos[i];
        o.id = i;
        o.price_ticks = 10'000 + static_cast<std::int64_t>(x % 1'000);
        o.qty = 1 + static_cast<std::int32_t>((x >> 16) % 500);
        o.side = static_cast<std::uint8_t>((x >> 32) & 1);
        hot[i] = OrderHot{o.price_ticks, o.qty, o.side};
        soa.price_ticks[i] = o.price_ticks;
        soa.qty[i] = o.qty;
        soa.side[i] = o.side;
    }
    const std::int64_t px = 10'500;
    std::printf("Scanning %ld orders (B/order: bytes the scan brings from memory):\n", n);
    bench("array of structs", 64, n, [&] { return scan_aos(aos, px); });
    bench("hot/cold split", 16, n, [&] { return scan_hot(hot, px); });
    bench("struct of arrays", 13, n, [&] { return scan_soa(soa, px); });
}
