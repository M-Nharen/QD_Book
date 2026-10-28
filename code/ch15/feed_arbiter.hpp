// A/B line arbitration and gap detection for a sequenced multicast feed (Section 15.2).
// Packets from both lines go through one Arbiter, which delivers every message exactly
// once and in sequence order, and reports a gap only when BOTH lines missed it.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include "feed_protocol.hpp"

namespace feed {

struct ArbStats {
    std::uint64_t delivered = 0, dup_pkts = 0, held_pkts = 0, late_pkts = 0;
    std::uint64_t gaps = 0, lost_msgs = 0, malformed = 0;
};

template <std::size_t Slots = 64>
class Arbiter {
public:
    // hold_limit: packets to wait for the other line before declaring a gap.
    Arbiter(std::uint64_t first_seq, unsigned hold_limit)
        : next_(first_seq), hold_limit_(hold_limit) {}

    // deliver(seq, payload) per new message; gap(from, to) for lost [from, to).
    template <class Deliver, class Gap>
    void on_packet(std::span<const std::byte> raw, Deliver&& deliver, Gap&& gap) {
        const auto v = parse(raw);
        if (!v) { ++st_.malformed; return; }
        const std::uint64_t end = v->seq + v->count;
        if (end <= next_) {                                    // nothing new
            if (v->count != 0) ++st_.dup_pkts;                 // (count 0 = heartbeat)
        } else if (v->seq <= next_) {                          // in order: the hot path
            emit(*v, deliver);
            if (held_ != 0) drain(deliver);                    // other line filled a hole
        } else {                                               // ahead: a hole before it
            hold(raw, *v);
        }
        if (held_ == 0) { waited_ = 0; return; }               // no hole open
        if (++waited_ > hold_limit_ || held_ == Slots) resolve(deliver, gap);
    }

    // End of stream (or a timer firing): give up on every remaining hole.
    template <class Deliver, class Gap>
    void flush(Deliver&& deliver, Gap&& gap) {
        while (held_ != 0) resolve(deliver, gap);
    }

    std::uint64_t next() const { return next_; }
    const ArbStats& stats() const { return st_; }

private:
    struct Slot {
        std::uint64_t seq = 0;
        std::size_t len = 0;                                   // 0 = free
        std::array<std::byte, kMaxPacket> data;
    };

    template <class Deliver>
    void emit(const PacketView& v, Deliver& deliver) {
        for_each_message(v, next_, [&](std::uint64_t s, std::span<const std::byte> m) {
            deliver(s, m);
            ++st_.delivered;
        });
        next_ = v.seq + v.count;
    }

    void hold(std::span<const std::byte> raw, const PacketView& v) {
        for (auto& s : pool_)
            if (s.len == 0) {
                std::memcpy(s.data.data(), raw.data(), raw.size());
                s.len = raw.size();
                s.seq = v.seq;
                ++held_;
                ++st_.held_pkts;
                return;
            }
    }

    Slot* oldest() {                                           // held packet, lowest seq
        Slot* best = nullptr;
        for (auto& s : pool_)
            if (s.len != 0 && (!best || s.seq < best->seq)) best = &s;
        return best;
    }

    template <class Deliver>
    void drain(Deliver& deliver) {                             // deliver what now fits
        while (Slot* s = oldest()) {
            if (s->seq > next_) return;                        // still a hole before it
            const auto v = parse({s->data.data(), s->len});    // validated in on_packet
            if (v->seq + v->count > next_) emit(*v, deliver);
            else if (v->count != 0) ++st_.late_pkts;           // both lines' copies held
            s->len = 0;
            --held_;
        }
    }

    template <class Deliver, class Gap>
    void resolve(Deliver& deliver, Gap& gap) {                 // declare the oldest hole
        const std::uint64_t to = oldest()->seq;
        gap(next_, to);                                        // ask for a retransmission
        ++st_.gaps;
        st_.lost_msgs += to - next_;
        next_ = to;
        drain(deliver);
        waited_ = 0;                                           // restart for the next hole
    }

    std::uint64_t next_;
    unsigned hold_limit_, waited_ = 0;
    std::size_t held_ = 0;
    ArbStats st_;
    std::array<Slot, Slots> pool_;                             // fixed: no allocation
};

}  // namespace feed
