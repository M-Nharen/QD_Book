// A sequenced multicast packet format modelled on MoldUDP64 (Section 15.2). All integers
// are big-endian ("network byte order"), as in most exchange feeds. The writing side
// (stores and a packet builder for the test senders) is in feed_build.hpp.
//   offset 0  session    10 bytes  (identifies the stream; constant within a day)
//   offset 10 seq        u64       sequence number of the FIRST message in the packet
//   offset 18 count      u16       number of messages in the packet
//   offset 20 messages   count x { u16 length, length bytes of payload }
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace feed {

inline constexpr std::size_t kHeader = 20;
inline constexpr std::size_t kMaxPacket = 1472;     // 1500-byte MTU - 20 IP - 8 UDP

inline std::uint64_t load_be64(const std::byte* p) {
    std::uint64_t v;
    std::memcpy(&v, p, 8);                            // memcpy, not a cast (11.3)
    return __builtin_bswap64(v);                      // x86 is little-endian: one bswap
}
inline std::uint16_t load_be16(const std::byte* p) {
    std::uint16_t v;
    std::memcpy(&v, p, 2);
    return __builtin_bswap16(v);
}

struct PacketView {
    std::uint64_t seq;                                // first message's sequence number
    std::uint16_t count;                              // messages [seq, seq + count)
    std::span<const std::byte> body;                  // the message blocks
};

// Validates lengths before trusting them: a short or corrupt datagram returns nullopt.
inline std::optional<PacketView> parse(std::span<const std::byte> pkt) {
    if (pkt.size() < kHeader || pkt.size() > kMaxPacket) return std::nullopt;
    PacketView v{load_be64(pkt.data() + 10), load_be16(pkt.data() + 18),
                 pkt.subspan(kHeader)};
    std::size_t off = 0;
    for (unsigned i = 0; i < v.count; ++i) {          // every block must fit
        if (off + 2 > v.body.size()) return std::nullopt;
        off += 2 + load_be16(v.body.data() + off);
        if (off > v.body.size()) return std::nullopt;
    }
    return v;
}

// Calls f(seq, payload) for messages with sequence number >= from (skips older ones).
template <class F>
void for_each_message(const PacketView& v, std::uint64_t from, F&& f) {
    std::size_t off = 0;
    for (unsigned i = 0; i < v.count; ++i) {
        const std::uint16_t len = load_be16(v.body.data() + off);
        if (v.seq + i >= from) f(v.seq + i, v.body.subspan(off + 2, len));
        off += 2 + len;
    }
}

}  // namespace feed
