// Optimized: the same checks composed at compile time with a fold expression (9.8).
#include <array>
#include <concepts>
#include <tuple>
#include "risk_common.hpp"

struct MaxQty {
    std::uint32_t max;
    bool ok(const Order& o) const { return o.qty <= max; }
};
struct PriceBand {
    std::int64_t ref, band;
    bool ok(const Order& o) const { return o.px >= ref - band && o.px <= ref + band; }
};
struct MaxNotional {
    std::int64_t max;
    bool ok(const Order& o) const { return o.px * o.qty <= max; }
};
struct SymbolAllowed {
    std::array<bool, 512> allowed;
    bool ok(const Order& o) const { return allowed[o.symbol]; }
};

template <class T>
concept Check = requires(const T& c, const Order& o) {
    { c.ok(o) } -> std::same_as<bool>;
};

template <Check... Cs>
class RiskPipeline {
public:
    explicit RiskPipeline(Cs... cs) : checks_(std::move(cs)...) {}
    bool ok(const Order& o) const {
        return std::apply([&](const Cs&... c) { return (c.ok(o) && ...); }, checks_);
    }
    bool ok_all(const Order& o) const {     // evaluates every check: no early exit
        return std::apply([&](const Cs&... c) { return (unsigned{c.ok(o)} & ...) != 0; },
                          checks_);
    }
    template <Check C> C& get() { return std::get<C>(checks_); }  // limits stay mutable
private:
    std::tuple<Cs...> checks_;
};

int main() {
    std::array<bool, 512> allowed;
    allowed.fill(true);
    for (int s = 0; s < 12; ++s) allowed[s] = false;

    // Which checks run, and in what order, is fixed at compile time; the limits are
    // ordinary run-time values (risk limits change intraday, so they must be).
    const Limits L = load_limits();
    RiskPipeline pipeline{SymbolAllowed{allowed}, MaxQty{L.max_qty},
                          PriceBand{L.ref_px, L.band}, MaxNotional{L.max_notional}};
    pipeline.get<MaxQty>().max = load_limits().max_qty;   // e.g. a limit update arrives

    const auto orders = make_orders(1 << 20);
    run("static pipeline (fold expression)", orders,
        [&](const Order& o) { return pipeline.ok(o); });
    run("static pipeline, evaluate all", orders,
        [&](const Order& o) { return pipeline.ok_all(o); });
}
