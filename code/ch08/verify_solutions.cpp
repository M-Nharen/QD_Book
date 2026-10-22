// Checks the answers to the Chapter 8 practice problems (Appendix A).
#include <cstdio>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

int failures = 0;
void check(const char* what, const char* got, const char* want) {
    const bool ok = std::strcmp(got, want) == 0;
    std::printf("  %-44s %-10s %s\n", what, got, ok ? "ok" : "MISMATCH");
    if (!ok) ++failures;
}
void check(const char* what, long got, long want) {
    std::printf("  %-44s %-10ld %s\n", what, got, got == want ? "ok" : "MISMATCH");
    if (got != want) ++failures;
}

#define CAT(...)                                                             \
    (std::is_lvalue_reference_v<decltype((__VA_ARGS__))>   ? "lvalue"        \
     : std::is_rvalue_reference_v<decltype((__VA_ARGS__))> ? "xvalue"        \
                                                            : "prvalue")

int copies = 0, moves = 0;
template <bool NoexceptMove> struct Tr {
    std::string s;
    explicit Tr(const char* v) : s(v) {}
    Tr(const Tr& o) : s(o.s) { ++copies; }
    Tr(Tr&& o) noexcept(NoexceptMove) : s(std::move(o.s)) { ++moves; }
};
using T = Tr<true>;
template <class F> void counts(const char* what, F f, long want_c, long want_m) {
    copies = moves = 0;
    f();
    char label[64];
    std::snprintf(label, sizeof label, "%s: copies", what);
    check(label, copies, want_c);
    std::snprintf(label, sizeof label, "%s: moves", what);
    check(label, moves, want_m);
}

struct Order { long id; };
std::string& ref_fn() { static std::string s; return s; }

T ret_prvalue() { return T{"a"}; }
T ret_local() { T t{"a"}; return t; }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpessimizing-move"
T ret_moved() { T t{"a"}; return std::move(t); }
#pragma GCC diagnostic pop
T ret_param(T p) { return p; }
struct Holder { T m{"m"}; T get() const { return m; } };
T ret_ternary(bool c) { T a{"a"}, b{"b"}; return c ? a : b; }
T ret_const_local() { const T t{"a"}; return t; }

template <class U> struct Deduced { using type = U; };
template <class U> Deduced<U> fwd(U&&) { return {}; }

int main() {
    std::printf("P8.1 value categories\n");
    {
        int&& x = 1;
        std::vector<int> v{1};
        Order o{1}, *p = &o;
        int arr[2] = {1, 2}, a = 1, b = 2;
        check("(a) x, declared int&&", CAT(x), "lvalue");
        check("(b) std::move(v)[0]", CAT(std::move(v)[0]), "lvalue");
        check("(c) ref_fn()", CAT(ref_fn()), "lvalue");
        check("(d) std::string(\"a\") + \"b\"", CAT(std::string("a") + "b"), "prvalue");
        check("(e) p->id", CAT(p->id), "lvalue");
        check("(f) static_cast<Order&&>(o).id", CAT(static_cast<Order&&>(o).id), "xvalue");
        check("(g) nullptr", CAT(nullptr), "prvalue");
        check("(h) a = b", CAT(a = b), "lvalue");
        check("(i) std::move(arr)[0]", CAT(std::move(arr)[0]), "xvalue");
        (void)x; (void)arr; (void)a; (void)b; (void)p;
    }

    std::printf("P8.2 push_back / emplace_back into a reserved vector\n");
    {
        std::vector<T> v;
        v.reserve(10);
        T t{"x"};
        counts("(a) push_back(t)", [&] { v.push_back(t); }, 1, 0);
        counts("(b) push_back(std::move(t))", [&] { v.push_back(std::move(t)); }, 0, 1);
        counts("(c) push_back(T{\"y\"})", [&] { v.push_back(T{"y"}); }, 0, 1);
        counts("(d) emplace_back(\"z\")", [&] { v.emplace_back("z"); }, 0, 0);
        T u{"u"};
        counts("(e) emplace_back(u)", [&] { v.emplace_back(u); }, 1, 0);
    }

    std::printf("P8.3 returns (GCC 13, -O2)\n");
    counts("(a) return T{...}", [] { T r = ret_prvalue(); }, 0, 0);
    counts("(b) return local (NRVO)", [] { T r = ret_local(); }, 0, 0);
    counts("(c) return std::move(local)", [] { T r = ret_moved(); }, 0, 1);
    counts("(d) return parameter", [] { T r = ret_param(T{"p"}); }, 0, 1);
    Holder h;
    counts("(e) return member", [&] { T r = h.get(); }, 1, 0);
    counts("(f) return c ? a : b", [] { T r = ret_ternary(true); }, 1, 0);
    counts("(g) return const local (NRVO)", [] { T r = ret_const_local(); }, 0, 0);

    std::printf("P8.4 deduction for template<class U> f(U&&)\n");
    {
        int i = 0;
        const int ci = 0;
        int& ri = i;
        static_assert(std::is_same_v<decltype(fwd(i))::type, int&>);
        static_assert(std::is_same_v<decltype(fwd(ci))::type, const int&>);
        static_assert(std::is_same_v<decltype(fwd(5))::type, int>);
        static_assert(std::is_same_v<decltype(fwd(std::move(ci)))::type, const int>);
        static_assert(std::is_same_v<decltype(fwd(ri))::type, int&>);
        check("five static_asserts compiled", 1, 1);
    }

    std::printf("P8.5 1000 push_back(T{...}) into an empty vector\n");
    counts("noexcept move", [] {
        std::vector<Tr<true>> v;
        for (int i = 0; i < 1000; ++i) v.push_back(Tr<true>{"x"});
    }, 0, 2023);
    counts("throwing move", [] {
        std::vector<Tr<false>> v;
        for (int i = 0; i < 1000; ++i) v.push_back(Tr<false>{"x"});
    }, 1023, 1000);

    std::printf("P8.7 sizes on x86-64 / libstdc++\n");
    {
        int a = 1, b = 2;
        auto lam = [&a, b] { return a + b; };
        check("optional<int>", long(sizeof(std::optional<int>)), 8);
        check("optional<double>", long(sizeof(std::optional<double>)), 16);
        check("optional<bool>", long(sizeof(std::optional<bool>)), 2);
        check("variant<int,double>", long(sizeof(std::variant<int, double>)), 16);
        check("variant<char,string>", long(sizeof(std::variant<char, std::string>)), 40);
        check("span<int>", long(sizeof(std::span<int>)), 16);
        check("span<int,4>", long(sizeof(std::span<int, 4>)), 8);
        check("string_view", long(sizeof(std::string_view)), 16);
        check("std::function<void()>", long(sizeof(std::function<void()>)), 32);
        check("lambda [&a, b] (int a, b)", long(sizeof lam), 16);
    }

    std::printf("P8.12 lambda state\n");
    {
        int x = 1;
        auto a = [x]() mutable { return ++x; };
        auto b = a;                                // copies the closure, including x
        a();
        a();
        const int ra = a(), rb = b();
        check("a() third call", ra, 4);
        check("b() first call", rb, 2);
        check("outer x", x, 1);
        auto c = [&x] { return x; };
        x = 10;
        check("[&x] after x = 10", c(), 10);
        auto d = [] { static int n = 0; return ++n; };
        auto e = d;                                // same closure type: same static
        const int rd = d(), re = e();
        check("d() then e(): d", rd, 1);
        check("d() then e(): e", re, 2);
    }
    std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
