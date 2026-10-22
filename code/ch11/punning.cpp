// Defined ways to reinterpret the bytes of one type as another (Section 11.3).
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

// C++20: std::bit_cast. Both types must be trivially copyable and the same size.
[[gnu::noipa]] std::uint64_t bits_bitcast(double d) {
    return std::bit_cast<std::uint64_t>(d);
}

// Pre-C++20 idiom: memcpy into a local. Compilers turn it into one register move.
[[gnu::noipa]] std::uint64_t bits_memcpy(double d) {
    std::uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    return u;
}

// Reading an integer from an arbitrary (possibly misaligned) position in a byte buffer.
template <class T>
    requires std::is_trivially_copyable_v<T>
T load(const unsigned char* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
[[gnu::noipa]] std::uint64_t load_u64_at(const unsigned char* buf, std::size_t off) {
    return load<std::uint64_t>(buf + off);
}

// Inspecting any object's bytes through unsigned char is explicitly allowed.
void dump_bytes(const void* obj, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(obj);
    for (std::size_t i = 0; i < n; ++i) std::printf("%02x ", b[i]);
    std::printf("\n");
}

int main() {
    const double px = 101.25;
    std::printf("bit_cast : %016llx\n", (unsigned long long)bits_bitcast(px));
    std::printf("memcpy   : %016llx\n", (unsigned long long)bits_memcpy(px));
    std::printf("bytes    : ");
    dump_bytes(&px, sizeof px);                    // little-endian: low byte first

    unsigned char wire[16];
    for (int i = 0; i < 16; ++i) wire[i] = static_cast<unsigned char>(0x10 + i);
    std::printf("load@3   : %016llx\n", (unsigned long long)load_u64_at(wire, 3));
    constexpr auto c = std::bit_cast<std::uint32_t>(1.0f);      // bit_cast is constexpr
    static_assert(c == 0x3f800000u);
    std::printf("1.0f     : %08x\n", c);
}
