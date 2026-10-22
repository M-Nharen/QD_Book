// The nanosecond penalty of modern C++ choices (Section 8.10): copies vs moves,
// three ways to call a callback, std::function construction, and variant dispatch.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

inline void escape(const void* p) { asm volatile("" : : "r"(p) : "memory"); }

template <class F> double ns_per(long n, F f) {
    for (long i = 0; i < n / 20; ++i) f();         // warm-up
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < n; ++i) f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
}

// Three ways to take a callback. The data and the work are identical.
template <class F> long sum_template(const std::vector<int>& v, F f) {
    long s = 0;
    for (int x : v) s += f(x);
    return s;
}
[[gnu::noinline]] long sum_fnptr(const std::vector<int>& v, long (*f)(int)) {
    long s = 0;
    for (int x : v) s += f(x);
    return s;
}
[[gnu::noinline]] long sum_function(const std::vector<int>& v,
                                    const std::function<long(int)>& f) {
    long s = 0;
    for (int x : v) s += f(x);
    return s;
}
long scale(int x) { return 3L * x + 1; }

struct A { long v; };
struct B { long v; };
struct C { long v; };
struct D { long v; };
using Var = std::variant<A, B, C, D>;
enum class Kind : std::uint8_t { A, B, C, D };
struct Tagged { Kind k; long v; };
struct Handle {                                    // different work per alternative
    long operator()(const A& a) const { return a.v; }
    long operator()(const B& b) const { return -b.v; }
    long operator()(const C& c) const { return 2 * c.v; }
    long operator()(const D& d) const { return d.v ^ 1; }
};

int main() {
    const std::string s100(100, 's');
    const std::vector<int> v1000(1000, 7);
    std::printf("Copy versus move (ns per operation):\n");
    std::string sa = s100, sb;
    std::vector<int> va = v1000, vb;
    std::printf("  string(100 chars): copy %6.1f   move %5.1f\n",
                ns_per(2'000'000, [&] { std::string c = s100; escape(c.data()); }),
                ns_per(2'000'000, [&] {                // two move-assignments per call:
                    sb = std::move(sa);                // the buffer goes back and forth,
                    sa = std::move(sb);                // nothing is allocated
                    escape(sa.data());
                }) / 2);
    std::printf("  vector<int>(1000): copy %6.1f   move %5.1f\n",
                ns_per(1'000'000, [&] { std::vector<int> c = v1000; escape(c.data()); }),
                ns_per(1'000'000, [&] {
                    vb = std::move(va);
                    va = std::move(vb);
                    escape(va.data());
                }) / 2);

    std::vector<int> data(4096);
    for (int i = 0; i < 4096; ++i) data[i] = i;
    const std::function<long(int)> fn = scale;
    long sink = 0;
    const double t_tmpl = ns_per(20'000, [&] {
        sink += sum_template(data, [](int x) { return 3L * x + 1; });
    }) / 4096;
    const double t_ptr = ns_per(20'000, [&] { sink += sum_fnptr(data, scale); }) / 4096;
    const double t_fn = ns_per(20'000, [&] { sink += sum_function(data, fn); }) / 4096;
    std::printf("Callback per element (ns): template %.2f  function pointer %.2f  "
                "std::function %.2f\n", t_tmpl, t_ptr, t_fn);

    struct Big { long a, b, c; };
    const Big big{1, 2, 3};
    const long a = 1, b = 2;
    std::printf("Construct+destroy std::function: 16-byte capture %.1f ns, "
                "24-byte capture %.1f ns\n",
                ns_per(2'000'000, [&] {
                    std::function<long()> f = [a, b] { return a * b; };
                    escape(&f);
                }),
                ns_per(2'000'000, [&] {
                    std::function<long()> f = [big] { return big.a + big.c; };
                    escape(&f);
                }));

    std::vector<Var> vars;
    std::vector<Tagged> tags;
    std::uint64_t x = 12345;
    for (int i = 0; i < 4096; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;           // random kinds: unpredictable
        switch (x % 4) {
        case 0: vars.emplace_back(A{i}); tags.push_back({Kind::A, i}); break;
        case 1: vars.emplace_back(B{i}); tags.push_back({Kind::B, i}); break;
        case 2: vars.emplace_back(C{i}); tags.push_back({Kind::C, i}); break;
        default: vars.emplace_back(D{i}); tags.push_back({Kind::D, i}); break;
        }
    }
    const double t_visit = ns_per(20'000, [&] {
        for (const Var& m : vars)
            sink += std::visit(Handle{}, m);
    }) / 4096;
    const double t_switch = ns_per(20'000, [&] {
        for (const Tagged& t : tags) {
            switch (t.k) {
            case Kind::A: sink += t.v; break;          // same work as Handle
            case Kind::B: sink += -t.v; break;
            case Kind::C: sink += 2 * t.v; break;
            case Kind::D: sink += t.v ^ 1; break;
            }
        }
    }) / 4096;
    std::printf("Dispatch per message (ns): std::visit %.2f   switch on enum %.2f\n",
                t_visit, t_switch);
    return sink == 42;
}
