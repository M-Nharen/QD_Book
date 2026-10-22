// Forwarding references, reference collapsing and std::forward (Section 8.3).
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

int copies = 0, moves = 0;
struct Tracer {
    std::string s;
    explicit Tracer(std::string v) : s(std::move(v)) {}
    Tracer(const Tracer& o) : s(o.s) { ++copies; }
    Tracer(Tracer&& o) noexcept : s(std::move(o.s)) { ++moves; }
};

template <class T> constexpr const char* name() {   // just enough names for this demo
    using U = std::remove_reference_t<T>;
    constexpr bool c = std::is_const_v<U>;
    if constexpr (std::is_lvalue_reference_v<T>) return c ? "const int&" : "int&";
    else if constexpr (std::is_rvalue_reference_v<T>) return c ? "const int&&" : "int&&";
    else return c ? "const int" : "int";
}

template <class T> void deduce(T&&) {                // T&& with T deduced: forwarding ref
    std::printf("  T = %-10s  parameter type T&& = %s\n", name<T>(), name<T&&>());
}

struct Pair {
    Tracer a, b;
    Pair(Tracer x, Tracer y) : a(std::move(x)), b(std::move(y)) {}
};
template <class T, class... A> T make_forgetful(A&&... args) {
    return T(args...);                               // every args is an lvalue: copies
}
template <class T, class... A> T make(A&&... args) {
    return T(std::forward<A>(args)...);              // restores each argument's category
}

void sink(Tracer t) { std::printf("    sink got %zu chars\n", t.s.size()); }
template <class A> void twice(A&& a) {
    sink(std::forward<A>(a));                        // moves out of a...
    sink(std::forward<A>(a));                        // ...so this gets a moved-from object
}

template <class F> void count(const char* what, F f) {
    copies = moves = 0;
    f();
    std::printf("  %-46s copies %d  moves %d\n", what, copies, moves);
}

int main() {
    int x = 1;
    const int cx = 2;
    std::printf("Deduction for template<class T> void deduce(T&&):\n");
    deduce(x);                                       // lvalue int
    deduce(cx);                                      // lvalue const int
    deduce(3);                                       // prvalue
    deduce(std::move(x));                            // xvalue
    deduce(std::move(cx));                           // const xvalue

    const std::string big(40, 'p');
    std::printf("Factories:\n");
    count("make_forgetful<Pair>(Tracer{..}, Tracer{..})",
          [&] { Pair p = make_forgetful<Pair>(Tracer{big}, Tracer{big}); });
    count("make<Pair>(Tracer{..}, Tracer{..})",
          [&] { Pair p = make<Pair>(Tracer{big}, Tracer{big}); });
    std::vector<Pair> v;
    v.reserve(4);
    Tracer t1{big}, t2{big};
    count("v.push_back(Pair(t1, t2))", [&] { v.push_back(Pair(t1, t2)); });
    count("v.emplace_back(t1, t2)", [&] { v.emplace_back(t1, t2); });
    count("v.emplace_back(std::move(t1), std::move(t2))",
          [&] { v.emplace_back(std::move(t1), std::move(t2)); });
    std::printf("Forwarding the same argument twice:\n");
    twice(Tracer{big});
}
