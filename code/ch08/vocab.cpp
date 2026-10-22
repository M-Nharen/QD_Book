// Vocabulary types: optional, variant, span and string_view (Section 8.8).
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct AddOrder { std::uint64_t id; std::int64_t px; std::uint32_t qty; };
struct Cancel { std::uint64_t id; };
struct Trade { std::int64_t px; std::uint32_t qty; };
using Msg = std::variant<AddOrder, Cancel, Trade>;

template <class... Fs> struct Overloaded : Fs... { using Fs::operator()...; };

// optional: "a value or nothing", stored inline. No heap, no sentinel values.
std::optional<std::int64_t> parse_int(std::string_view s) {
    std::int64_t v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size()) return std::nullopt;
    return v;
}

// span: a pointer and a length, over any contiguous range.
std::int64_t notional(std::span<const std::int64_t> px, std::span<const std::int64_t> qty) {
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < px.size() && i < qty.size(); ++i) sum += px[i] * qty[i];
    return sum;
}

int main() {
    std::printf("sizeof: optional<int> %zu, optional<double> %zu, optional<bool> %zu, "
                "optional<string> %zu\n", sizeof(std::optional<int>),
                sizeof(std::optional<double>), sizeof(std::optional<bool>),
                sizeof(std::optional<std::string>));
    std::printf("sizeof: variant<int,double> %zu, Msg %zu (largest member %zu)\n",
                sizeof(std::variant<int, double>), sizeof(Msg), sizeof(AddOrder));
    std::printf("sizeof: span<int> %zu, span<int,4> %zu, string_view %zu, string %zu\n",
                sizeof(std::span<int>), sizeof(std::span<int, 4>),
                sizeof(std::string_view), sizeof(std::string));

    for (std::string_view s : {"10137", "10x37", ""}) {
        const auto v = parse_int(s);
        std::printf("parse_int(\"%.*s\") -> %s %lld\n", int(s.size()), s.data(),
                    v ? "value" : "nullopt", static_cast<long long>(v.value_or(-1)));
    }

    // string_view: slicing without copying. The bytes stay where they were.
    const std::string line = "55=AAPL|44=10137|38=100";
    std::string_view rest = line;
    while (!rest.empty()) {
        const std::size_t bar = rest.find('|');
        const std::string_view field = rest.substr(0, bar);   // O(1): pointer + length
        const std::size_t eq = field.find('=');
        std::printf("  tag %.*s value %.*s\n", int(eq), field.data(),
                    int(field.size() - eq - 1), field.data() + eq + 1);
        rest = bar == std::string_view::npos ? std::string_view{} : rest.substr(bar + 1);
    }

    // span works with arrays, vectors and sub-ranges alike.
    const std::array<std::int64_t, 4> px = {100, 101, 102, 103};
    const std::vector<std::int64_t> qty = {5, 6, 7, 8};
    std::printf("notional(all) = %lld, notional(first two) = %lld\n",
                static_cast<long long>(notional(px, qty)),
                static_cast<long long>(notional(std::span(px).first(2), qty)));

    // variant: a closed set of types, one active at a time, dispatched with visit.
    const std::vector<Msg> msgs = {AddOrder{1, 10137, 100}, Trade{10137, 40}, Cancel{1}};
    std::int64_t volume = 0;
    int adds = 0, cancels = 0;
    const auto handler = Overloaded{                   // one lambda per alternative;
        [&](const AddOrder&) { ++adds; },              // a missing case fails to compile
        [&](const Cancel&) { ++cancels; },
        [&](const Trade& t) { volume += t.qty; },
    };
    for (const Msg& m : msgs) std::visit(handler, m);
    std::printf("adds %d, cancels %d, traded volume %lld\n", adds, cancels,
                static_cast<long long>(volume));
    std::printf("msgs[1].index() = %zu, holds Trade: %d\n", msgs[1].index(),
                int(std::holds_alternative<Trade>(msgs[1])));
    if (const Cancel* c = std::get_if<Cancel>(&msgs[2]))   // nullptr if not a Cancel
        std::printf("get_if<Cancel>: id %llu\n", static_cast<unsigned long long>(c->id));
}
