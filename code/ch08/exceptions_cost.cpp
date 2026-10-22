// What exceptions and noexcept cost (Section 8.6): the happy path, the throwing path at
// two stack depths, the error-code alternative, and vector growth with and without a
// noexcept move constructor. Timings are machine-dependent.
#include <chrono>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

volatile int g_side;                               // destructors write here: real work
struct Guard { ~Guard() { g_side = g_side + 1; } };
struct RejectError { int code; };                  // small exception type: no string

[[gnu::noipa]] int dive_throw(int depth, bool fail) {
    Guard g;                                       // a destructor to run in every frame
    if (depth == 0) {
        if (fail) throw RejectError{42};
        return 1;
    }
    return dive_throw(depth - 1, fail) + 1;
}
[[gnu::noipa]] int dive_code(int depth, bool fail, int& err) {
    Guard g;
    if (depth == 0) {
        if (fail) { err = 42; return -1; }
        return 1;
    }
    const int r = dive_code(depth - 1, fail, err);
    return r < 0 ? r : r + 1;                      // every level checks and propagates
}

template <class F> double ns_per(int n, F f) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
}

template <bool NoexceptMove> struct Elem {
    std::string s = std::string(40, 'e');          // heap-owning: copies allocate
    static inline int copies = 0;
    Elem() = default;
    Elem(const Elem& o) : s(o.s) { ++copies; }
    Elem(Elem&& o) noexcept(NoexceptMove) : s(std::move(o.s)) {}
};

template <bool B> void grow(const char* label) {
    Elem<B>::copies = 0;
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Elem<B>> v;
    for (int i = 0; i < 1'000'000; ++i) v.emplace_back();
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("  %-24s nothrow-movable=%d  copies %7d  %6.1f ms\n", label,
                int(std::is_nothrow_move_constructible_v<Elem<B>>), Elem<B>::copies,
                std::chrono::duration<double, std::milli>(t1 - t0).count());
}

int main() {
    int err = 0;
    long sink = 0;
    std::printf("Per call (ns), recursion depth 1 and 20, a destructor in every frame:\n");
    constexpr int N = 1'000'000;
    for (int d : {1, 20}) {
        const double ok_throw = ns_per(N, [&] { sink += dive_throw(d, false); });
        const double ok_code = ns_per(N, [&] { sink += dive_code(d, false, err); });
        const double bad_code = ns_per(N, [&] { sink += dive_code(d, true, err); });
        const double bad_throw = ns_per(N / 10, [&] {
            try { sink += dive_throw(d, true); } catch (const RejectError& e) {
                sink += e.code;
            }
        });
        std::printf(" depth %2d  success: may-throw fn %7.1f   error-code fn %7.1f\n"
                    "           failure: throw+catch  %7.1f   error code    %7.1f\n",
                    d, ok_throw, ok_code, bad_throw, bad_code);
    }
    std::printf("vector<Elem>, 1,000,000 emplace_back with no reserve():\n");
    grow<true>("Elem(Elem&&) noexcept");
    grow<false>("Elem(Elem&&) may throw");
    return sink == 0;
}
