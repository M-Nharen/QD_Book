// book: flags=-pthread
// Showdown, optimized side (Section 15.6): non-blocking socket polled in a loop, bound to
// the group, 8 MiB receive buffer, recvmmsg() fetching up to 64 datagrams per system
// call into preallocated buffers, messages parsed in place, gaps detected. tsan-clean.
#include <memory>
#include "feed_arbiter.hpp"
#include "showdown_harness.hpp"

void receive(Harness& h) {
    const int fd = mc::open_rx(Harness::kGroup, Harness::kPort, "127.0.0.1", 8 << 20);
    constexpr unsigned kBatch = 64;
    static std::byte bufs[kBatch][2048];              // allocated once, outside the loop
    static iovec iov[kBatch];
    static mmsghdr msgs[kBatch];
    for (unsigned i = 0; i < kBatch; ++i) {
        iov[i] = {bufs[i], sizeof bufs[i]};
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
    auto arb = std::make_unique<feed::Arbiter<>>(1, 32);
    auto deliver = [&h](std::uint64_t seq, std::span<const std::byte> m) {
        h.on_message(seq, m);
    };
    h.gaps_seen = 0;
    auto gap = [&h](std::uint64_t, std::uint64_t) { ++h.gaps_seen; };   // would request
    h.ready.store(true, std::memory_order_release);                    // a retransmission
    while (!h.done.load(std::memory_order_acquire)) {
        const int n = recvmmsg(fd, msgs, kBatch, MSG_DONTWAIT, nullptr);
        for (int i = 0; i < n; ++i)
            arb->on_packet({bufs[i], msgs[i].msg_len}, deliver, gap);
    }
    close(fd);
}

int main() {
    Harness h;
    h.run("optimized receiver", receive);
}
