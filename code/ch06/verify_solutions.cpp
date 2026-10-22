// Checks every numeric or printed answer in the Chapter 6 solutions (Appendix A).
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <variant>

static int failures = 0;
template <class A, class B>
void check(const char* what, A got, B want) {
    const bool ok = got == static_cast<A>(want);
    failures += !ok;
    std::printf("%-56s %s\n", what, ok ? "ok" : "MISMATCH");
}
long diff(const void* a, const void* b) {
    return static_cast<const char*>(b) - static_cast<const char*>(a);
}

// P6.1
struct A1 { char c; double d; short s; };
struct B1 { short s; char c; int i; char d; };
struct C1 { char c[3]; std::int16_t x; };
struct D1 { int a; std::int64_t b; bool f; };
struct E1 { long double x; char c; };
// P6.2
struct QuoteNaive {
    bool valid; std::int64_t bid; std::int32_t bid_qty; char venue[3];
    std::int64_t ask; std::int32_t ask_qty; std::uint16_t flags;
};
struct QuoteSorted {
    std::int64_t bid, ask; std::int32_t bid_qty, ask_qty; std::uint16_t flags;
    char venue[3]; bool valid;
};
// P6.3
struct X {};
struct Y { X x; int i; };
struct Z : X { int i; };
struct W : X { X x; int i; };
// P6.4
struct A4 { virtual ~A4() = default; int a = 0; };
struct B4 { virtual ~B4() = default; double b = 0; };
struct C4 : A4, B4 { char c = 0; };
// P6.5
struct P5 { virtual ~P5() = default; int x = 0; };
struct Q5 : P5 { int y = 0; };
struct P5b { long v; int x; };
struct Q5b : P5b { int y; };
// P6.6
std::string trace;
struct Base6 {
    Base6() { hello(); }
    virtual ~Base6() { hello(); }
    virtual void hello() { trace += "Base "; }
};
struct Derived6 : Base6 {
    Derived6() { hello(); }
    ~Derived6() override { hello(); }
    void hello() override { trace += "Derived "; }
};
// P6.8
#pragma pack(push, 1)
struct WireTrade {
    char type; std::uint64_t ts_ns; char symbol[8]; std::uint32_t price; std::uint32_t qty;
};
#pragma pack(pop)
// P6.11
struct Stats { std::atomic<long> produced{0}; std::atomic<long> consumed{0}; };
struct StatsPadded {
    alignas(64) std::atomic<long> produced{0};
    alignas(64) std::atomic<long> consumed{0};
};

int main() {
    check("P6.1 sizeof A1 = 24", sizeof(A1), 24);
    check("P6.1 sizeof B1 = 12", sizeof(B1), 12);
    check("P6.1 sizeof C1 = 6, alignof 2", sizeof(C1) * 10 + alignof(C1), 62);
    check("P6.1 sizeof D1 = 24", sizeof(D1), 24);
    check("P6.1 sizeof E1 = 32, alignof 16", sizeof(E1) * 100 + alignof(E1), 3216);
    check("P6.2 naive Quote = 40", sizeof(QuoteNaive), 40);
    check("P6.2 sorted Quote = 32", sizeof(QuoteSorted), 32);
    check("P6.3 X=1 Y=8 Z=4 W=8",
          sizeof(X) * 1000 + sizeof(Y) * 100 + sizeof(Z) * 10 + sizeof(W), 1848);
    C4 c;
    check("P6.4 sizeof C4 = 40", sizeof(C4), 40);
    check("P6.4 B4 subobject at +16", diff(&c, static_cast<B4*>(&c)), 16);
    check("P6.4 c.c at +32", diff(&c, &c.c), 32);
    Q5 q;
    check("P6.5 sizeof P5 = 16, Q5 = 16", sizeof(P5) * 100 + sizeof(Q5), 1616);
    check("P6.5 Q5::y at +12 (in P5's tail padding)", diff(&q, &q.y), 12);
    check("P6.5 sizeof P5b = 16, Q5b = 24", sizeof(P5b) * 100 + sizeof(Q5b), 1624);
    { Derived6 d; }
    std::printf("P6.6 printed: %s\n", trace.c_str());
    check("P6.6 Base Derived Derived Base", trace == "Base Derived Derived Base ", true);
    check("P6.7 variant<char, int16_t> = 4", sizeof(std::variant<char, std::int16_t>), 4);
    check("P6.7 variant<int64_t, double, char> = 16",
          sizeof(std::variant<std::int64_t, double, char>), 16);
    check("P6.7 optional<double> = 16", sizeof(std::optional<double>), 16);
    check("P6.7 optional<int32_t> = 8", sizeof(std::optional<std::int32_t>), 8);
    check("P6.7 variant<string, int> = 40", sizeof(std::variant<std::string, int>), 40);
    check("P6.7 unique_ptr<int, void(*)(int*)> = 16",
          sizeof(std::unique_ptr<int, void (*)(int*)>), 16);
    check("P6.8 sizeof WireTrade = 25", sizeof(WireTrade), 25);
    check("P6.8 price at offset 17", offsetof(WireTrade, price), 17);
    const long n = 10'000'000, aos = n * 64, soa = n * (8 + 4 + 1);
    const long gbps = 10'000'000'000;            // 10 GB/s, in bytes per second
    check("P6.9 AoS bytes = 640 MB", aos, 640'000'000L);
    check("P6.9 SoA bytes = 130 MB", soa, 130'000'000L);
    check("P6.9 AoS time at 10 GB/s = 64 ms", aos * 1000 / gbps, 64L);
    check("P6.9 SoA time at 10 GB/s = 13 ms", soa * 1000 / gbps, 13L);
    check("P6.11 sizeof Stats = 16", sizeof(Stats), 16);
    check("P6.11 sizeof StatsPadded = 128", sizeof(StatsPadded), 128);
    std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
    return failures != 0;
}
