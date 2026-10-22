// CORRECT: same message format as Listing 11.4, decoded without undefined behaviour.
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

template <class T>
    requires std::is_integral_v<T>
T load_le(const unsigned char* p) {                   // little-endian, any alignment
    unsigned char b[sizeof(T)];
    std::memcpy(b, p, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) std::reverse(b, b + sizeof(T));
    return std::bit_cast<T>(b);
}

struct AddOrder {                                     // natural layout for the hot path
    std::uint64_t order_id;
    std::int64_t price;                               // widened on decode
    std::int64_t qty;
    std::uint32_t instrument;
    char side;
};
constexpr std::size_t kWireSize = 22;

inline AddOrder decode(const unsigned char* p) {
    return AddOrder{load_le<std::uint64_t>(p + 2), load_le<std::int32_t>(p + 10),
                    load_le<std::int32_t>(p + 14), load_le<std::uint32_t>(p + 18),
                    static_cast<char>(p[1])};
}

[[gnu::noipa]] long long total_notional(std::span<const unsigned char> buf) {
    long long total = 0;
    const std::size_t n_msgs = buf.size() / kWireSize;  // never read past the end
    for (std::size_t k = 0; k < n_msgs; ++k) {
        const AddOrder m = decode(buf.data() + kWireSize * k);
        const long long notional = m.price * m.qty;     // 64-bit multiply: cannot overflow
        total += (m.side == 'B') ? notional : -notional;
    }
    return total;
}

// Test-data writer (assumes a little-endian host, checked below).
void encode(unsigned char* p, char side, std::uint64_t id, std::int32_t px, std::int32_t q,
            std::uint32_t ins) {
    static_assert(std::endian::native == std::endian::little);
    p[0] = 'A';
    p[1] = static_cast<unsigned char>(side);
    std::memcpy(p + 2, &id, 8);
    std::memcpy(p + 10, &px, 4);
    std::memcpy(p + 14, &q, 4);
    std::memcpy(p + 18, &ins, 4);
}

int main() {
    constexpr std::size_t N = 1 << 20;
    std::vector<unsigned char> buf(kWireSize * N);
    std::uint64_t s = 12345;                          // same test data as Listing 11.4
    for (std::size_t k = 0; k < N; ++k) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        encode(buf.data() + kWireSize * k, (s >> 63) ? 'B' : 'S', k,
               9000 + int((s >> 40) % 2000), 1 + int((s >> 20) % 500), 7);
    }
    long long r = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 50; ++rep) r += total_notional(buf);
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / (50.0 * N);
    std::printf("checksum %lld, %.2f ns/message\n", r, ns);
}
