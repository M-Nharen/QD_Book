// The initialization forms you will be asked about (Section 5.4).
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

struct Order {                              // an aggregate: public members, no ctors
    std::int64_t id{};                      // {} = default member initializer (zero);
    std::int64_t price_ticks{};             // the type is still an aggregate
    std::int32_t qty{};
    char side{};
};

struct Raw { int x; int y; };               // no initializers: default-init is garbage

struct Config {
    int depth = 10;                         // default member initializers
    bool log_fills = true;
    std::string venue = "SIM";
};

int trace(const char* name, int v) { std::printf("  init %s\n", name); return v; }

#pragma GCC diagnostic ignored "-Wreorder"  // only so -Werror lets us show the trap
struct Member {
    int a, b;                               // declaration order decides init order
    Member() : b(trace("b", 1)), a(trace("a", 2)) {}   // the list order is ignored
};

std::int64_t g_counter;                     // static storage: zero-initialized

int main() {
    Order o1{1, 10'050, 100, 'B'};          // aggregate init, by position
    Order o2{.id = 2, .qty = 5};            // C++20 designated init; others use {}
    Order o3{};                             // value-init: every member zero
    Raw r{};                                // value-init zeroes even without {} members
    // Raw g;  int bad = g.x;               // default-init: reading g.x is UB (11.2)
    std::printf("o1 %lld %lld %d %c\n", (long long)o1.id, (long long)o1.price_ticks,
                o1.qty, o1.side);
    std::printf("o2 %lld %lld %d %d\n", (long long)o2.id, (long long)o2.price_ticks,
                o2.qty, o2.side);
    std::printf("o3 %lld %lld %d %d\n", (long long)o3.id, (long long)o3.price_ticks,
                o3.qty, o3.side);

    Config c;                               // default-init runs the member initializers
    std::printf("raw %d %d\n", r.x, r.y);
    std::printf("config %d %d %s, g_counter %lld\n", c.depth, c.log_fills,
                c.venue.c_str(), (long long)g_counter);

    std::vector<int> v1(3, 7);              // parentheses: 3 copies of 7
    std::vector<int> v2{3, 7};              // braces prefer initializer_list: {3, 7}
    std::printf("v1.size() = %zu, v2.size() = %zu\n", v1.size(), v2.size());

    auto a1 = {42};                         // std::initializer_list<int>
    auto a2{42};                            // int (since C++17)
    std::printf("a1 is a list of %zu, a2 is an int: %d\n", a1.size(), a2);

    double px = 3.7;
    // int bad{px};                         // error: narrowing conversion in braces
    int truncated(px);                      // parentheses accept it: 3.7 becomes 3
    std::printf("truncated = %d\n", truncated);

    std::printf("Member():\n");
    Member m;
    std::printf("  a = %d, b = %d\n", m.a, m.b);

    auto zeroed = std::make_unique<char[]>(1 << 20);             // value-init: memset
    auto raw = std::make_unique_for_overwrite<char[]>(1 << 20);  // C++20: no memset
    raw[0] = zeroed[0];
    std::printf("zeroed[12345] = %d\n", zeroed[12345]);
}
