// Copy elision, RVO, NRVO and implicit move on return: which returns copy, which
// move, and which do neither (Section 8.4).
#include <cstdio>
#include <string>
#include <utility>

int copies = 0, moves = 0;
struct T {
    std::string s;
    explicit T(const char* v) : s(v) {}
    T(const T& o) : s(o.s) { ++copies; }
    T(T&& o) noexcept : s(std::move(o.s)) { ++moves; }
};
struct Pinned {                                    // neither copyable nor movable
    int v;
    explicit Pinned(int x) : v(x) {}
    Pinned(const Pinned&) = delete;
    Pinned(Pinned&&) = delete;
};

T g_t{"global"};

T prvalue() { return T{"a"}; }                     // C++17: guaranteed elision
Pinned make_pinned() { return Pinned{7}; }         // so this compiles at all
T nrvo() { T t{"a"}; t.s += "b"; return t; }       // named return value: may elide
T two_names(bool b) {
    T x{"x"}, y{"y"};
    if (b) return x;                               // two candidates: cannot place both
    return y;                                      // in the return slot -> implicit move
}
T from_param(T t) { return t; }                    // parameters are never elided
T from_rref(T&& t) { return t; }                   // C++20: implicitly moved
T from_global() { return g_t; }                    // not a local: copy
T ternary(bool b) {
    T x{"x"}, y{"y"};
    return b ? x : y;                              // an expression, not a name: copy
}
struct Two { T first{"p"}, second{"q"}; };
T binding() {
    Two two;
    auto& [first, second] = two;
    return first;                                  // a structured binding: copy
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpessimizing-move"
T pessimized() { T t{"a"}; return std::move(t); }  // defeats NRVO: one extra move
#pragma GCC diagnostic pop
void by_value(T t) { (void)t; }

template <class F> void count(const char* what, F f) {
    copies = moves = 0;
    f();
    std::printf("  %-42s copies %d  moves %d\n", what, copies, moves);
}

int main() {
    count("T t = prvalue()", [] { T t = prvalue(); });
    count("Pinned p = make_pinned()", [] { Pinned p = make_pinned(); (void)p; });
    count("T t = T(T(T(\"a\")))", [] { T t = T(T(T("a"))); });
    count("T t = nrvo()", [] { T t = nrvo(); });
    count("T t = two_names(true)", [] { T t = two_names(true); });
    count("T t = from_param(T{\"a\"})", [] { T t = from_param(T{"a"}); });
    count("T t = from_rref(T{\"a\"})", [] { T t = from_rref(T{"a"}); });
    count("T t = from_global()", [] { T t = from_global(); });
    count("T t = ternary(true)", [] { T t = ternary(true); });
    count("T t = binding()", [] { T t = binding(); });
    count("T t = pessimized()", [] { T t = pessimized(); });
    count("by_value(T{\"a\"})", [] { by_value(T{"a"}); });
}
