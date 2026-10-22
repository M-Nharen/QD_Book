// Binary floating point vs. decimal prices, and a fixed-point alternative (Section 5.7).
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

// Parse a non-negative decimal like "1234.5" into integer units of 10^-4 without ever
// creating a double. Rejects more than 4 decimals instead of rounding silently.
std::optional<std::int64_t> parse_e4(std::string_view s) {
    std::int64_t whole = 0, frac = 0;
    const char* p = s.data();
    const char* end = s.data() + s.size();
    auto r = std::from_chars(p, end, whole);
    if (r.ec != std::errc{} || whole < 0 || whole > INT64_MAX / 10'000) return std::nullopt;
    p = r.ptr;
    int digits = 0;
    if (p != end && *p == '.') {
        for (++p; p != end && *p >= '0' && *p <= '9'; ++p, ++digits) {
            if (digits == 4) return std::nullopt;
            frac = frac * 10 + (*p - '0');
        }
    }
    if (p != end) return std::nullopt;
    for (; digits < 4; ++digits) frac *= 10;
    return whole * 10'000 + frac;
}

int main() {
    std::printf("0.1 + 0.2 = %.17g, == 0.3? %s\n", 0.1 + 0.2,
                0.1 + 0.2 == 0.3 ? "yes" : "no");

    const double tick = 0.05, px = 1.15;
    const double ratio = px / tick;
    std::printf("1.15 / 0.05 = %.17g -> cast %ld, llround %lld\n", ratio,
                static_cast<long>(ratio), std::llround(ratio));
    std::printf("23 * 0.05 = %.17g, literal 1.15 = %.17g, equal? %s\n", 23 * tick, px,
                23 * tick == px ? "yes" : "no");

    const double two53 = 9007199254740992.0;     // 2^53
    std::printf("2^53 + 1 = %.17g (the +1 is lost)\n", two53 + 1.0);
    std::printf("(1e16 + 1) - 1e16 = %g, but (1e16 - 1e16) + 1 = %g\n",
                (1e16 + 1.0) - 1e16, (1e16 - 1e16) + 1.0);

    double total = 0.0;                          // a running P&L in currency units
    std::int64_t total_e4 = 0;                   // the same P&L in units of 10^-4
    for (int i = 0; i < 10'000'000; ++i) {
        total += 0.01;
        total_e4 += 100;
    }
    std::printf("10^7 x 0.01: double %.10f, fixed-point %lld.%04lld\n", total,
                static_cast<long long>(total_e4 / 10'000),
                static_cast<long long>(total_e4 % 10'000));

    const double nan = std::nan("");
    std::printf("NaN == NaN? %s; -0.0 == 0.0? %s; signbit(-0.0) = %d\n",
                nan == nan ? "yes" : "no", -0.0 == 0.0 ? "yes" : "no",
                std::signbit(-0.0) ? 1 : 0);

    const std::int64_t tick_e4 = 500;            // tick size 0.05 in units of 10^-4
    for (std::string_view s : {"1.15", "1234.5", "0.0499", "1.12345", "12a"}) {
        auto v = parse_e4(s);
        if (!v) {
            std::printf("parse \"%.*s\": rejected\n", int(s.size()), s.data());
            continue;
        }
        std::printf("parse \"%.*s\": %lld e-4, on tick grid? %s, ticks %lld\n",
                    int(s.size()), s.data(), static_cast<long long>(*v),
                    *v % tick_e4 == 0 ? "yes" : "no", static_cast<long long>(*v / tick_e4));
    }
}
