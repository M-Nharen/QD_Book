// Exceptions for rare, fatal problems; return values for expected outcomes (Section 5.8).
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

struct Order { std::int64_t price_ticks; std::int32_t qty; };
struct Limits { std::int64_t max_px; std::int32_t max_qty; };

enum class Reject : std::uint8_t { None, BadPrice, BadQty, TooLarge };

const char* to_string(Reject r) {
    switch (r) {
        case Reject::None:     return "accepted";
        case Reject::BadPrice: return "bad price";
        case Reject::BadQty:   return "bad quantity";
        case Reject::TooLarge: return "over limit";
    }
    return "?";
}

// Hot path: a rejected order is a normal business outcome, so it is a return value.
// [[nodiscard]] makes ignoring the result a compiler warning (an error under -Werror).
[[nodiscard]] Reject validate(const Order& o, const Limits& lim) noexcept {
    if (o.price_ticks <= 0) return Reject::BadPrice;
    if (o.qty <= 0) return Reject::BadQty;
    if (o.price_ticks > lim.max_px || o.qty > lim.max_qty) return Reject::TooLarge;
    return Reject::None;
}

// Start-up path: a broken configuration means we must not trade at all. An exception
// carries a message up to main, and the cost of throwing is irrelevant here.
Limits load_limits(const std::string& max_qty_text) {
    std::size_t used = 0;
    const long q = std::stol(max_qty_text, &used);      // throws on non-numbers
    if (used != max_qty_text.size() || q <= 0 || q > 1'000'000)
        throw std::invalid_argument("max_qty must be in 1..10^6: " + max_qty_text);
    return Limits{1'000'000, static_cast<std::int32_t>(q)};
}

int main() {
    for (const char* text : {"500", "50O"}) {           // the second has a letter O
        try {
            const Limits lim = load_limits(text);
            for (Order o : {Order{10'050, 100}, Order{0, 1}, Order{10'050, 900}})
                std::printf("limit %s: order %lld x %d -> %s\n", text,
                            static_cast<long long>(o.price_ticks), o.qty,
                            to_string(validate(o, lim)));
        } catch (const std::exception& e) {
            std::printf("config error, refusing to start: %s\n", e.what());
        }
    }
}
