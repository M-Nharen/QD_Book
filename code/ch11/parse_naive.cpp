// NAIVE: decoding a binary add-order message by casting the receive buffer (Sec. 11.7).
// Illustrative wire format (not any exchange's): 22 bytes, little-endian, no padding.
//   0 type 'A' | 1 side 'B'/'S' | 2 order_id u64 | 10 price i32 ticks | 14 qty i32
//   18 instrument u32
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma pack(push, 1)
struct AddOrderWire {                       // mirrors the wire layout byte for byte
    char type;
    char side;
    std::uint64_t order_id;
    std::int32_t price;
    std::int32_t qty;
    std::uint32_t instrument;
};
#pragma pack(pop)
static_assert(sizeof(AddOrderWire) == 22);

[[gnu::noipa]] long long total_notional(const unsigned char* buf, std::size_t n_msgs) {
    long long total = 0;
    for (std::size_t k = 0; k < n_msgs; ++k) {
        auto* m = reinterpret_cast<const AddOrderWire*>(buf + 22 * k);  // UB: no object
        long long notional = m->price * m->qty;          // BUG: int * int, may overflow
        total += (m->side == 'B') ? notional : -notional;
    }
    return total;
}

int main() {
    constexpr std::size_t N = 1 << 20;
    std::vector<unsigned char> buf(22 * N);
    std::uint64_t s = 12345;                              // tiny LCG for test data
    for (std::size_t k = 0; k < N; ++k) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        AddOrderWire w{'A', (s >> 63) ? 'B' : 'S', k, 9000 + int((s >> 40) % 2000),
                       1 + int((s >> 20) % 500), 7};
        std::memcpy(buf.data() + 22 * k, &w, sizeof w);
    }
    long long r = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 50; ++rep) r += total_notional(buf.data(), N);
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / (50.0 * N);
    std::printf("checksum %lld, %.2f ns/message\n", r, ns);
}
