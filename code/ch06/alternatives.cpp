// book: timeout=240
// One job, six dispatch mechanisms: value a mixed portfolio of instruments (6.6).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <variant>
#include <vector>

// 1. Virtual functions, objects owned through pointers (the classic OO design).
struct Instrument {
    virtual ~Instrument() = default;
    virtual long value(long mid) const = 0;
};
struct VEquity final : Instrument { long value(long m) const override { return m; } };
struct VFuture final : Instrument {
    long basis;
    explicit VFuture(long b) : basis(b) {}
    long value(long m) const override { return m + basis; }
};
struct VOption final : Instrument {
    long delta;
    explicit VOption(long d) : delta(d) {}
    long value(long m) const override { return (m * delta) >> 10; }
};

// 2. std::variant of plain value types, stored contiguously.
struct Equity { long value(long m) const { return m; } };
struct Future { long basis; long value(long m) const { return m + basis; } };
struct Option { long delta; long value(long m) const { return (m * delta) >> 10; } };
using AnyInstrument = std::variant<Equity, Future, Option>;

// 3. A hand-written tagged struct and a switch.
enum class Kind : std::uint8_t { Equity, Future, Option };
struct Tagged { Kind kind; long param; };
inline long value_of(const Tagged& t, long m) {
    switch (t.kind) {
        case Kind::Equity: return m;
        case Kind::Future: return m + t.param;
        case Kind::Option: return (m * t.param) >> 10;
    }
    return 0;
}

// 4. A table of function pointers indexed by the tag.
using ValueFn = long (*)(long param, long m);
constexpr ValueFn kTable[3] = {
    [](long, long m) { return m; },
    [](long p, long m) { return m + p; },
    [](long p, long m) { return (m * p) >> 10; },
};

template <class F>
void bench(const char* name, const char* bytes_per_item, long n, F f) {
    double best = 1e30;
    long sum = 0;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        sum = f();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
        best = ns < best ? ns : best;
    }
    std::printf("  %-32s %-6s %6.2f ns/item  (sum %ld)\n", name, bytes_per_item, best, sum);
}

int main() {
    const long n = 1'000'000;
    std::vector<std::unique_ptr<Instrument>> virt;
    std::vector<AnyInstrument> var;
    std::vector<Tagged> tagged;
    std::vector<std::function<long(long)>> funcs;
    std::vector<Equity> equities;                // 6. grouped by type
    std::vector<Future> futures;
    std::vector<Option> options;
    std::uint64_t x = 2463534242ULL;
    for (long i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        const long k = static_cast<long>(x % 3), p = 1 + static_cast<long>((x >> 8) % 1000);
        if (k == 0) {
            virt.push_back(std::make_unique<VEquity>());
            var.emplace_back(Equity{});
            funcs.emplace_back([](long m) { return m; });
            equities.push_back(Equity{});
        } else if (k == 1) {
            virt.push_back(std::make_unique<VFuture>(p));
            var.emplace_back(Future{p});
            funcs.emplace_back([p](long m) { return m + p; });
            futures.push_back(Future{p});
        } else {
            virt.push_back(std::make_unique<VOption>(p));
            var.emplace_back(Option{p});
            funcs.emplace_back([p](long m) { return (m * p) >> 10; });
            options.push_back(Option{p});
        }
        tagged.push_back(Tagged{static_cast<Kind>(k), p});
    }
    const long mid = 100'000;
    static_assert(sizeof(AnyInstrument) == 16 && sizeof(Tagged) == 16);
    std::printf("Valuing %ld instruments in random type order:\n", n);
    bench("1 virtual, vector<unique_ptr>", "8+32", n, [&] {
        long s = 0;
        for (const auto& p : virt) s += p->value(mid);
        return s;
    });
    bench("2 std::variant + std::visit", "16", n, [&] {
        long s = 0;
        for (const auto& v : var)
            s += std::visit([&](const auto& i) { return i.value(mid); }, v);
        return s;
    });
    bench("3 tag + switch", "16", n, [&] {
        long s = 0;
        for (const auto& t : tagged) s += value_of(t, mid);
        return s;
    });
    bench("4 tag + function-pointer table", "16", n, [&] {
        long s = 0;
        for (const auto& t : tagged) s += kTable[static_cast<int>(t.kind)](t.param, mid);
        return s;
    });
    bench("5 std::function", "32", n, [&] {
        long s = 0;
        for (const auto& f : funcs) s += f(mid);
        return s;
    });
    bench("6 grouped by type, no dispatch", "1-8", n, [&] {
        long s = 0;
        for (const auto& e : equities) s += e.value(mid);
        for (const auto& f : futures) s += f.value(mid);
        for (const auto& o : options) s += o.value(mid);
        return s;
    });
}
