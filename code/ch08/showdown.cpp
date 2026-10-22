// Code showdown (Section 8.9): decoding and dispatching tag=value messages.
// Naive: owning strings, a map per message, exceptions for errors, std::function lookup.
// Modern: string_view slicing, from_chars, error codes, static dispatch, no allocation.
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Totals { long orders = 0, cancels = 0, rejects = 0, qty = 0, px = 0; };

// ------------------------------------------------------------------ naive
struct NaiveOrder { std::string symbol; long price = 0; int qty = 0; char side = 0; };

std::vector<std::string> split(std::string s, char d) {        // by value: a copy
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) { out.push_back(cur); cur.clear(); } else { cur += c; }
    }
    out.push_back(cur);
    return out;
}

struct NaiveDecoder {
    Totals t;
    std::map<std::string, std::function<void(const std::map<std::string, std::string>&)>>
        handlers;
    NaiveDecoder() {
        handlers["D"] = [this](const auto& kv) {
            NaiveOrder o{kv.at("55"), std::stol(kv.at("44")), std::stoi(kv.at("38")),
                         kv.at("54")[0]};                       // throws if missing/bad
            ++t.orders; t.qty += o.qty; t.px += o.price;
        };
        handlers["F"] = [this](const auto& kv) {
            (void)std::stol(kv.at("41"));
            ++t.cancels;
        };
    }
    void on_message(std::string msg) {                          // by value: a copy
        try {
            std::map<std::string, std::string> kv;
            for (const std::string& f : split(msg, '|')) {
                const auto eq = f.find('=');
                kv[f.substr(0, eq)] = f.substr(eq + 1);
            }
            handlers.at(kv.at("35"))(kv);
        } catch (const std::exception&) {
            ++t.rejects;
        }
    }
};

// ------------------------------------------------------------------ modern
enum class Status : std::uint8_t { Ok, BadNumber, MissingField, UnknownType };
struct NewOrder { std::uint32_t symbol; std::int64_t price; std::uint32_t qty; char side; };
struct CancelOrder { std::uint64_t orig_id; };

template <class Int> bool to_int(std::string_view s, Int& out) noexcept {
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && end == s.data() + s.size() && !s.empty();
}

constexpr std::string_view kSymbols[] = {"AAPL", "MSFT", "NVDA", "AMZN"};

// Decodes one message and calls h.on_new / h.on_cancel. Handler is a template
// parameter: the calls are direct and inlinable. Nothing here allocates or throws.
template <class Handler> Status decode(std::string_view msg, Handler& h) noexcept {
    char type = 0, side = 0;
    std::int64_t price = -1;
    std::uint32_t qty = 0, sym = ~0u;
    std::uint64_t orig = 0;
    bool have_orig = false;
    while (!msg.empty()) {
        const std::size_t bar = msg.find('|');
        const std::string_view f = msg.substr(0, bar);
        msg = bar == std::string_view::npos ? std::string_view{} : msg.substr(bar + 1);
        const std::size_t eq = f.find('=');
        if (eq == std::string_view::npos) return Status::BadNumber;
        int tag = 0;
        if (!to_int(f.substr(0, eq), tag)) return Status::BadNumber;
        const std::string_view v = f.substr(eq + 1);
        switch (tag) {
        case 35: type = v.empty() ? 0 : v[0]; break;
        case 54: side = v.empty() ? 0 : v[0]; break;
        case 44: if (!to_int(v, price)) return Status::BadNumber; break;
        case 38: if (!to_int(v, qty)) return Status::BadNumber; break;
        case 41: if (!to_int(v, orig)) return Status::BadNumber; have_orig = true; break;
        case 55:
            for (std::uint32_t i = 0; i < std::size(kSymbols); ++i)
                if (v == kSymbols[i]) sym = i;
            break;
        default: break;                                         // ignore unknown tags
        }
    }
    if (type == 'D') {
        if (price < 0 || qty == 0 || sym == ~0u || side == 0) return Status::MissingField;
        h.on_new(NewOrder{sym, price, qty, side});
        return Status::Ok;
    }
    if (type == 'F') {
        if (!have_orig) return Status::MissingField;
        h.on_cancel(CancelOrder{orig});
        return Status::Ok;
    }
    return Status::UnknownType;
}

struct Counter {
    Totals t;
    void on_new(const NewOrder& o) { ++t.orders; t.qty += o.qty; t.px += o.price; }
    void on_cancel(const CancelOrder&) { ++t.cancels; }
};

// ------------------------------------------------------------------ driver
std::vector<std::string> make_messages(int n, int bad_every) {
    std::vector<std::string> v;
    std::uint64_t x = 88172645463325252ULL;                     // xorshift64: fixed data
    for (int i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        const std::string px = std::to_string(10000 + x % 500);
        if (bad_every && i % bad_every == bad_every - 1)
            v.push_back("35=D|55=AAPL|44=10137|54=1");           // malformed: no 38=qty
        else if (x % 4 == 0)
            v.push_back("35=F|41=" + std::to_string(x % 100000));
        else
            v.push_back("35=D|55=" + std::string(kSymbols[x % 4]) + "|44=" + px +
                        "|38=" + std::to_string(1 + x % 300) + (x & 1 ? "|54=1" : "|54=2"));
    }
    return v;
}

template <class F> double ns_per_msg(const std::vector<std::string>& msgs, F f) {
    const auto t0 = std::chrono::steady_clock::now();
    for (const std::string& m : msgs) f(m);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(msgs.size());
}

int main() {
    for (int bad_every : {0, 100, 1}) {                // none, 1 in 100, every message
        const auto msgs = make_messages(200'000, bad_every);
        NaiveDecoder naive;
        Counter modern;
        long modern_rejects = 0;
        const double tn = ns_per_msg(msgs, [&](const std::string& m) {
            naive.on_message(m);
        });
        const double tm = ns_per_msg(msgs, [&](const std::string& m) {
            if (decode(m, modern) != Status::Ok) ++modern_rejects;
        });
        std::printf("%3d%% malformed: naive %7.1f ns/msg, modern %6.1f ns/msg\n",
                    bad_every ? 100 / bad_every : 0, tn, tm);
        std::printf("  same results? orders %ld/%ld cancels %ld/%ld rejects %ld/%ld "
                    "qty %s\n", naive.t.orders, modern.t.orders, naive.t.cancels,
                    modern.t.cancels, naive.t.rejects, modern_rejects,
                    naive.t.qty == modern.t.qty && naive.t.px == modern.t.px ? "equal"
                                                                             : "DIFFER");
    }
}
