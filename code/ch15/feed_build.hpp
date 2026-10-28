// Writing side of the packet format of feed_protocol.hpp (Section 15.2), used by the
// test senders and the showdown harness: big-endian stores and a packet builder.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include "feed_protocol.hpp"

namespace feed {

inline void store_be64(std::byte* p, std::uint64_t v) {
    v = __builtin_bswap64(v);
    std::memcpy(p, &v, 8);
}
inline void store_be16(std::byte* p, std::uint16_t v) {
    v = __builtin_bswap16(v);
    std::memcpy(p, &v, 2);
}

// Builds a packet; returns its size. Used by the test senders.
inline std::size_t build(std::byte* out, std::uint64_t seq,
                         std::span<const std::span<const std::byte>> msgs) {
    std::memcpy(out, "SESSION001", 10);
    store_be64(out + 10, seq);
    store_be16(out + 18, static_cast<std::uint16_t>(msgs.size()));
    std::size_t off = kHeader;
    for (auto m : msgs) {
        store_be16(out + off, static_cast<std::uint16_t>(m.size()));
        std::memcpy(out + off + 2, m.data(), m.size());
        off += 2 + m.size();
    }
    return off;
}

}  // namespace feed
