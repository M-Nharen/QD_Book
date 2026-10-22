// Naive: pre-trade checks as a runtime list of virtual objects (Section 9.8).
#include <array>
#include <memory>
#include <string_view>
#include "risk_common.hpp"

struct RiskCheck {
    virtual ~RiskCheck() = default;
    virtual bool ok(const Order& o) const = 0;
};
struct MaxQty : RiskCheck {
    std::uint32_t max;
    explicit MaxQty(std::uint32_t m) : max(m) {}
    bool ok(const Order& o) const override { return o.qty <= max; }
};
struct PriceBand : RiskCheck {
    std::int64_t ref, band;
    PriceBand(std::int64_t r, std::int64_t b) : ref(r), band(b) {}
    bool ok(const Order& o) const override {
        return o.px >= ref - band && o.px <= ref + band;
    }
};
struct MaxNotional : RiskCheck {
    std::int64_t max;
    explicit MaxNotional(std::int64_t m) : max(m) {}
    bool ok(const Order& o) const override { return o.px * o.qty <= max; }
};
struct SymbolAllowed : RiskCheck {
    std::array<bool, 512> allowed{};
    SymbolAllowed() { allowed.fill(true); for (int s = 0; s < 12; ++s) allowed[s] = false; }
    bool ok(const Order& o) const override { return allowed[o.symbol]; }
};

int main() {
    // The list is assembled at run time from configuration text, so the compiler cannot
    // know which checks exist or in what order.
    const Limits L = load_limits();
    std::vector<std::unique_ptr<RiskCheck>> checks;
    for (std::string_view name : {"symbol", "qty", "band", "notional"}) {
        std::unique_ptr<RiskCheck> c;
        if (name == "symbol")   c = std::make_unique<SymbolAllowed>();
        if (name == "qty")      c = std::make_unique<MaxQty>(L.max_qty);
        if (name == "band")     c = std::make_unique<PriceBand>(L.ref_px, L.band);
        if (name == "notional") c = std::make_unique<MaxNotional>(L.max_notional);
        checks.push_back(std::move(c));
    }
    const auto orders = make_orders(1 << 20);
    run("virtual list (vector<unique_ptr>)", orders, [&](const Order& o) {
        for (const auto& c : checks)
            if (!c->ok(o)) return false;    // one indirect call per check
        return true;
    });
}
