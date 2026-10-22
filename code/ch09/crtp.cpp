// CRTP: a reusable feed-handler base that calls into its derived class statically (9.5).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

struct AddMsg   { std::uint64_t id; std::int64_t px; std::uint32_t qty; char side; };
struct CancelMsg { std::uint64_t id; };
struct TradeMsg { std::uint64_t id; std::uint32_t qty; };

template <class Derived>
class FeedHandler {
public:
    // Parses [type:1][len:1][payload:len] records and dispatches each one.
    std::size_t on_packet(const std::uint8_t* p, std::size_t n) {
        std::size_t off = 0, handled = 0;
        while (off + 2 <= n) {
            const std::uint8_t type = p[off], len = p[off + 1];
            const std::uint8_t* body = p + off + 2;
            if (off + 2 + len > n) break;                    // truncated record
            switch (type) {
            case 'A': self().on_add(decode<AddMsg>(body, len)); break;
            case 'X': self().on_cancel(decode<CancelMsg>(body, len)); break;
            case 'T': self().on_trade(decode<TradeMsg>(body, len)); break;
            default:  self().on_unknown(type); break;
            }
            off += 2 + len;
            ++handled;
        }
        return handled;
    }

    // Default hooks. A derived class "overrides" one by declaring a member with the same
    // name; name lookup in self() finds the derived one first. No virtual, no vtable.
    void on_trade(const TradeMsg&) {}
    void on_unknown(std::uint8_t) {}

private:
    FeedHandler() = default;
    friend Derived;                 // only Derived can construct FeedHandler<Derived>

    Derived& self() { return static_cast<Derived&>(*this); }

    template <class M>
    static M decode(const std::uint8_t* body, std::size_t len) {
        M m{};                                               // memcpy, not a pointer cast:
        std::memcpy(&m, body, len < sizeof m ? len : sizeof m); // no aliasing/alignment UB
        return m;                                            // (Section 11.3)
    }
};

class BookBuilder : public FeedHandler<BookBuilder> {
public:
    void on_add(const AddMsg& m)       { ++adds; volume += m.qty; }
    void on_cancel(const CancelMsg&)   { ++cancels; }
    void on_trade(const TradeMsg& m)   { traded += m.qty; }  // hides the default
    long adds = 0, cancels = 0, volume = 0, traded = 0;
};

// class Oops : public FeedHandler<BookBuilder> {};   // copy-paste bug: wrong argument.
// Oops o;   // error: FeedHandler<BookBuilder>() is private and Oops is not a friend

template <class M>
void append(std::vector<std::uint8_t>& out, char type, const M& m) {
    out.push_back(static_cast<std::uint8_t>(type));
    out.push_back(static_cast<std::uint8_t>(sizeof m));
    const auto* b = reinterpret_cast<const std::uint8_t*>(&m);   // reading bytes via
    out.insert(out.end(), b, b + sizeof m);                       // unsigned char: OK
}

int main() {
    std::vector<std::uint8_t> pkt;
    append(pkt, 'A', AddMsg{1, 1'000'500, 100, 'B'});
    append(pkt, 'A', AddMsg{2, 1'000'600, 250, 'S'});
    append(pkt, 'T', TradeMsg{2, 50});
    append(pkt, 'X', CancelMsg{1});
    append(pkt, 'Z', CancelMsg{9});                          // unknown type: default hook
    BookBuilder bb;
    const std::size_t n = bb.on_packet(pkt.data(), pkt.size());
    std::printf("records=%zu adds=%ld cancels=%ld volume=%ld traded=%ld\n",
                n, bb.adds, bb.cancels, bb.volume, bb.traded);
    std::printf("sizeof(BookBuilder)=%zu: no vptr\n", sizeof(BookBuilder));
}
