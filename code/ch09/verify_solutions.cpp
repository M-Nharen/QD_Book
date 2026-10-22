// Checks the answers to the Chapter 9 practice problems (Appendix A, Section A.9).
// Most checks are static_asserts: if an answer were wrong, this file would not compile.
#include <array>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ---- P9.1: specialization binds to the primary template visible at its declaration.
namespace p1a {
template <class T> const char* f(T)  { return "f(T)"; }
template <class T> const char* f(T*) { return "f(T*)"; }
template <> const char* f(char*)     { return "f<char>(char*) specialization"; }
const char* f(int)                   { return "f(int) non-template"; }
}
namespace p1b {
template <class T> const char* f(T)  { return "f(T)"; }
template <> const char* f(char*)     { return "f<char*>(char*) specialization"; }
template <class T> const char* f(T*) { return "f(T*)"; }
}

// ---- P9.3
template <class T, class... Ts>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Ts> || ...);
template <class T, class... Ts>
inline constexpr bool all_same_v = (std::is_same_v<T, Ts> && ...);
static_assert(is_one_of_v<int, char, int> && !is_one_of_v<int> && !is_one_of_v<int, long>);
static_assert(all_same_v<int> && all_same_v<int, int, int> && !all_same_v<int, int&>);

// ---- P9.4
template <class T, template <class...> class Tmpl>
struct is_specialization_of : std::false_type {};
template <template <class...> class Tmpl, class... Args>
struct is_specialization_of<Tmpl<Args...>, Tmpl> : std::true_type {};
static_assert(is_specialization_of<std::vector<int>, std::vector>::value);
static_assert(!is_specialization_of<std::list<int>, std::vector>::value);
static_assert(is_specialization_of<std::tuple<>, std::tuple>::value);

// ---- P9.5 (part a; part b is a compile error, checked separately)
template <class T> concept Small = sizeof(T) <= 4;
template <class T> requires std::integral<T>             int g(T) { return 1; }
template <class T> requires std::integral<T> && Small<T> int g(T) { return 2; }

// ---- P9.6: CRC-32 (reflected polynomial 0xEDB88320), table built at compile time.
constexpr auto kCrcTable = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
    }
    return t;
}();
constexpr std::uint32_t crc32(std::string_view s) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (char ch : s) c = kCrcTable[(c ^ static_cast<unsigned char>(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
consteval std::uint32_t crc32_ct(std::string_view s) { return crc32(s); }
static_assert(crc32_ct("123456789") == 0xCBF43926u);    // the standard check value

// ---- P9.7
template <class Tuple, std::size_t... I>
constexpr auto reverse_impl(Tuple&& t, std::index_sequence<I...>) {
    constexpr std::size_t N = sizeof...(I);
    return std::make_tuple(std::get<N - 1 - I>(std::forward<Tuple>(t))...);
}
template <class... Ts>
constexpr auto tuple_reverse(const std::tuple<Ts...>& t) {
    return reverse_impl(t, std::index_sequence_for<Ts...>{});
}
static_assert(tuple_reverse(std::tuple{1, 2.5, 'c'}) == std::tuple{'c', 2.5, 1});
static_assert(tuple_reverse(std::tuple<>{}) == std::tuple<>{});

// ---- P9.8
template <class Derived>
class InstanceCounter {
public:
    static long live() { return live_; }
private:
    InstanceCounter() { ++live_; }
    InstanceCounter(const InstanceCounter&) { ++live_; }
    ~InstanceCounter() { --live_; }
    friend Derived;
    static inline long live_ = 0;            // one counter per Derived
};
struct Widget : InstanceCounter<Widget> {};
struct Gadget : InstanceCounter<Gadget> {};

// ---- P9.9
long g_calls = 0;
long fib_calls(int n) { ++g_calls; return n < 2 ? n : fib_calls(n - 1) + fib_calls(n - 2); }
template <int N> struct Fib { static constexpr long v = Fib<N - 1>::v + Fib<N - 2>::v; };
template <> struct Fib<1> { static constexpr long v = 1; };
template <> struct Fib<0> { static constexpr long v = 0; };
static_assert(Fib<30>::v == 832040);

// ---- P9.10: smallest odd multiplier giving a collision-free table of size 8.
constexpr std::uint32_t fnv1a(std::string_view s) {
    std::uint32_t h = 2166136261u;
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
    return h;
}
constexpr std::array<std::string_view, 8> kKeys{"NewOrder", "Cancel", "Replace", "Trade",
                                                "Heartbeat", "Logon", "Logout", "Reject"};
consteval std::uint32_t find_multiplier(int lg) {
    for (std::uint32_t a = 1; a < 100'000; a += 2) {
        std::uint64_t used = 0;
        bool ok = true;
        for (auto k : kKeys) {
            const std::uint32_t slot = (fnv1a(k) * a) >> (32 - lg);
            if ((used >> slot) & 1) { ok = false; break; }
            used |= std::uint64_t{1} << slot;
        }
        if (ok) return a;
    }
    return 0;
}
constexpr std::uint32_t kMul8 = find_multiplier(3);
constexpr std::uint32_t kMul16 = find_multiplier(4);

// ---- P9.11: a 256-entry table of function pointers generated from the message list.
struct Add    { static constexpr char kType = 'A'; };
struct Cancel { static constexpr char kType = 'X'; };
struct Trade  { static constexpr char kType = 'T'; };
int g_hits[3];
template <class M> void handle(const unsigned char*);
template <> void handle<Add>(const unsigned char*)    { ++g_hits[0]; }
template <> void handle<Cancel>(const unsigned char*) { ++g_hits[1]; }
template <> void handle<Trade>(const unsigned char*)  { ++g_hits[2]; }
void unknown(const unsigned char*) {}

using Fn = void (*)(const unsigned char*);
template <class... Msgs>
constexpr std::array<Fn, 256> make_table() {
    std::array<Fn, 256> t{};
    t.fill(&unknown);
    ((t[static_cast<unsigned char>(Msgs::kType)] = &handle<Msgs>), ...);
    return t;
}
constexpr auto kTable = make_table<Add, Cancel, Trade>();

int main() {
    char c = 'x';
    int i = 0;
    std::printf("P9.1 order 1: f(1)=%s | f(&i)=%s\n", p1a::f(1), p1a::f(&i));
    std::printf("              f(&c)=%s | f(1.5)=%s\n", p1a::f(&c), p1a::f(1.5));
    std::printf("P9.1 order 2: f(&c)=%s\n", p1b::f(&c));
    std::printf("P9.5 g(1)=%d g(1L)=%d g(short)=%d\n", g(1), g(1L), g(short{1}));
    std::printf("P9.6 crc32(\"123456789\") = 0x%08X\n", crc32("123456789"));
    {
        Widget w1, w2;
        Gadget g1;
        {
            Widget w3 = w1;
            std::printf("P9.8 inner: widgets=%ld gadgets=%ld\n", Widget::live(),
                        Gadget::live());
        }
        std::printf("P9.8 outer: widgets=%ld gadgets=%ld\n", Widget::live(),
                    Gadget::live());
    }
    std::printf("P9.8 end:   widgets=%ld\n", Widget::live());
    const long f30 = fib_calls(30);
    std::printf("P9.9 fib(30)=%ld, calls=%ld, Fib<30>::v=%ld\n", f30, g_calls, Fib<30>::v);
    std::printf("P9.10 multiplier for m=8: %u, for m=16: %u\n", kMul8, kMul16);
    for (auto k : kKeys)
        std::printf("      %-9.*s -> slot %u\n", int(k.size()), k.data(),
                    (fnv1a(k) * kMul8) >> 29);
    const unsigned char msg[] = "AXTTZA";
    for (int k = 0; k < 6; ++k) kTable[msg[k]](msg);
    std::printf("P9.11 adds=%d cancels=%d trades=%d\n", g_hits[0], g_hits[1], g_hits[2]);
}
