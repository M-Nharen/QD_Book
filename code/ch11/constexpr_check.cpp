// Using constant evaluation as a UB detector (Section 11.6): the compiler must reject any
// constant expression whose evaluation has undefined behaviour, so edge-case tests that run
// at compile time prove the absence of UB on exactly those inputs.
#include <climits>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <optional>

// Rounds towards a, like std::midpoint.
constexpr int midpoint_safe(int a, int b) {
    using U = unsigned;
    const U diff = a > b ? U(a) - U(b) : U(b) - U(a); // unsigned arithmetic wraps: defined
    const U half = diff / 2;
    return a > b ? int(U(a) - half) : int(U(a) + half);
}

constexpr std::optional<int> checked_add(int a, int b) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return std::nullopt;
    return a + b;                                     // only reached when it fits
}

constexpr std::optional<int> checked_abs(int x) {
    if (x == INT_MIN) return std::nullopt;            // -INT_MIN is not representable
    return x < 0 ? -x : x;
}

consteval bool self_test() {
    const int edge[] = {INT_MIN, INT_MIN + 1, -2, -1, 0, 1, 2, INT_MAX - 1, INT_MAX};
    for (int a : edge)
        for (int b : edge) {
            if (midpoint_safe(a, b) != std::midpoint(a, b)) return false;
            auto s = checked_add(a, b);
            const long long wide = (long long)a + b;
            if (s.has_value() != (wide >= INT_MIN && wide <= INT_MAX)) return false;
            if (s && *s != wide) return false;
        }
    return !checked_abs(INT_MIN) && *checked_abs(-5) == 5;
}
static_assert(self_test());   // any UB inside self_test() would be a compile error

int main() {
    std::printf("midpoint_safe(INT_MAX, INT_MAX - 2) = %d\n",
                midpoint_safe(INT_MAX, INT_MAX - 2));
    std::printf("midpoint_safe(INT_MIN, INT_MAX)     = %d\n",
                midpoint_safe(INT_MIN, INT_MAX));
    std::printf("checked_add(INT_MAX, 1) has value?  = %d\n",
                checked_add(INT_MAX, 1).has_value());
    std::printf("all compile-time edge-case tests passed\n");
}
