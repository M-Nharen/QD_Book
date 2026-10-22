// Every expression has a type and a value category; decltype((e)) reveals the category
// (Section 8.1).
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

// decltype of a parenthesized expression: T& for lvalues, T&& for xvalues, T for prvalues.
#define CATEGORY(...)                                                        \
    std::printf("  %-26s %s\n", #__VA_ARGS__,                                \
                std::is_lvalue_reference_v<decltype((__VA_ARGS__))>   ? "lvalue"  \
                : std::is_rvalue_reference_v<decltype((__VA_ARGS__))> ? "xvalue"  \
                                                                       : "prvalue")

struct Quote { long px = 0; std::string venue; };

int value() { return 1; }
int& ref(int& x) { return x; }
int&& rref(int& x) { return std::move(x); }
Quote make_quote() { return Quote{}; }

void take(int&) { std::printf("    -> take(int&)\n"); }
void take(const int&) { std::printf("    -> take(const int&)\n"); }
void take(int&&) { std::printf("    -> take(int&&)\n"); }

int main() {
    int x = 5;
    int&& r = 7;                     // r is declared as an rvalue reference...
    [[maybe_unused]] int arr[3] = {1, 2, 3};   // used only inside decltype
    Quote q;
    std::printf("Expression                   category\n");
    CATEGORY(x);
    CATEGORY(r);                     // ...but naming it gives an lvalue
    CATEGORY(42);
    CATEGORY(x + 1);
    CATEGORY(++x);
    CATEGORY(x++);
    CATEGORY(std::move(x));
    CATEGORY(static_cast<int&&>(x)); // what std::move is
    CATEGORY(value());
    CATEGORY(ref(x));
    CATEGORY(rref(x));
    CATEGORY("literal");             // an lvalue: an array with static storage
    CATEGORY(arr[0]);
    CATEGORY(q.px);
    CATEGORY(make_quote());
    CATEGORY(make_quote().px);       // member of a prvalue: materialized, then xvalue
    CATEGORY(std::move(q).venue);
    CATEGORY(x > 0 ? x : arr[1]);    // both operands lvalues: lvalue
    CATEGORY(x > 0 ? x : 3);         // mixed: prvalue

    std::printf("Overload resolution follows the category:\n");
    const int c = 1;
    take(x);                         // lvalue        -> int&
    take(c);                         // const lvalue  -> const int&
    take(42);                        // prvalue       -> int&&
    take(std::move(x));              // xvalue        -> int&&
    take(r);                         // named rvalue reference is an lvalue -> int&
}
