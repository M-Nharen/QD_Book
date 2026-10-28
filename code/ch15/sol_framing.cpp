// Solution to P15.9: reassembling length-prefixed messages from a TCP byte stream.
// Each message is a big-endian u16 length followed by that many bytes. read() may return
// any number of bytes, splitting messages anywhere, so the reader buffers partial input.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <random>
#include <span>
#include <vector>

class FrameReader {
public:
    static constexpr std::size_t kMaxMsg = 4096;               // the protocol's limit

    // Feeds the next chunk of the stream; calls on_msg(payload) for each complete
    // message. Returns false if the stream is corrupt (a length above kMaxMsg).
    template <class F>
    bool feed(std::span<const std::byte> chunk, F&& on_msg) {
        while (!chunk.empty()) {
            const std::size_t take = std::min(chunk.size(), sizeof buf_ - used_);
            std::memcpy(buf_ + used_, chunk.data(), take);    // append what fits
            used_ += take;
            chunk = chunk.subspan(take);
            std::size_t off = 0;
            while (used_ - off >= 2) {                         // complete frames
                const std::size_t len = (std::to_integer<std::size_t>(buf_[off]) << 8) |
                                        std::to_integer<std::size_t>(buf_[off + 1]);
                if (len > kMaxMsg) return false;                // corrupt: resync or drop
                if (used_ - off < 2 + len) break;               // wait for the rest
                on_msg(std::span<const std::byte>(buf_ + off + 2, len));
                off += 2 + len;
            }
            std::memmove(buf_, buf_ + off, used_ - off);       // keep the partial tail
            used_ -= off;
        }
        return true;
    }

private:
    std::byte buf_[2 + kMaxMsg];                                 // one maximal frame
    std::size_t used_ = 0;
};

int main() {
    std::mt19937 rng(9);
    std::vector<std::vector<std::byte>> sent;                 // 100,000 random messages
    std::vector<std::byte> stream;
    for (int i = 0; i < 100'000; ++i) {
        std::vector<std::byte> m(rng() % 300);                 // includes empty messages
        for (auto& b : m) b = std::byte(rng());
        stream.push_back(std::byte(m.size() >> 8));
        stream.push_back(std::byte(m.size() & 0xff));
        stream.insert(stream.end(), m.begin(), m.end());
        sent.push_back(std::move(m));
    }
    for (int max_chunk : {1, 7, 1460, 65536}) {                // how read() might split it
        static FrameReader r;
        r = FrameReader{};
        std::size_t got = 0, bad = 0;
        auto on_msg = [&](std::span<const std::byte> m) {
            if (got >= sent.size() || m.size() != sent[got].size() ||
                !std::equal(m.begin(), m.end(), sent[got].begin())) ++bad;
            ++got;
        };
        for (std::size_t pos = 0; pos < stream.size();) {
            const std::size_t n = std::min<std::size_t>(1 + rng() % max_chunk,
                                                        stream.size() - pos);
            if (!r.feed({stream.data() + pos, n}, on_msg)) { ++bad; break; }
            pos += n;
        }
        std::printf("chunks up to %5d bytes: %zu messages reassembled, %zu mismatches\n",
                    max_chunk, got, bad);
    }
}
