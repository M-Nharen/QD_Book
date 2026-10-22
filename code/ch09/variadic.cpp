// Parameter packs, pack expansion, fold expressions and a compile-time dispatcher (9.6).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

// Folds. (... op e) is a left fold, (e op ...) a right fold, (e op ... op init) has a seed.
template <class... Ts> constexpr auto sum(Ts... xs) { return (xs + ... + 0); }
template <class... Ts> constexpr bool all_positive(Ts... xs) { return ((xs > 0) && ...); }
static_assert(sum() == 0 && sum(1, 2, 3) == 6);
static_assert(all_positive() && !all_positive(1, -2, 3));   // empty && fold is true

void print_one(int x)         { std::printf("%d", x); }
void print_one(double x)      { std::printf("%.2f", x); }
void print_one(const char* s) { std::printf("\"%s\"", s); }

template <class... Ts>
void print_line(const Ts&... xs) {
    int i = 0;
    ((std::fputs(i++ ? ", " : "", stdout), print_one(xs)), ...);  // comma fold, in order
    std::printf("  (%zu args)\n", sizeof...(Ts));
}

// Type-list queries computed by the compiler.
template <class T, class... Ts>
inline constexpr bool contains = (std::is_same_v<T, Ts> || ...);

template <class T, class... Ts>
constexpr std::size_t index_of() {
    std::size_t i = 0;
    const bool found = ((std::is_same_v<T, Ts> ? true : (++i, false)) || ...);
    return found ? i : sizeof...(Ts);
}
static_assert(contains<int, char, int> && !contains<long, char, int>);
static_assert(index_of<double, int, char, double>() == 2);

// Iterating a tuple: expand an index_sequence 0..N-1 into a pack of indices.
template <class Tuple, class F, std::size_t... I>
void for_each_impl(Tuple& t, F&& f, std::index_sequence<I...>) {
    (f(std::get<I>(t)), ...);
}
template <class... Ts, class F>
void for_each(std::tuple<Ts...>& t, F&& f) {
    for_each_impl(t, f, std::index_sequence_for<Ts...>{});
}

// A dispatcher generated from a list of message types. Each type carries its wire tag.
struct Add    { static constexpr char kType = 'A'; std::uint64_t id; std::uint32_t qty; };
struct Cancel { static constexpr char kType = 'X'; std::uint64_t id; };
struct Trade  { static constexpr char kType = 'T'; std::uint64_t id; std::uint32_t qty; };

template <class... Msgs, class Handler>
bool dispatch(char type, const unsigned char* body, Handler&& h) {
    static_assert(sizeof...(Msgs) > 0);
    auto try_one = [&]<class M>(std::type_identity<M>) {
        if (type != M::kType) return false;
        M m;
        std::memcpy(&m, body, sizeof m);
        h(m);
        return true;
    };
    return (try_one(std::type_identity<Msgs>{}) || ...);    // stops at the first match
}

struct Counter {
    long adds = 0, cancels = 0, qty = 0;
    void operator()(const Add& a)    { ++adds; qty += a.qty; }
    void operator()(const Cancel&)   { ++cancels; }
    void operator()(const Trade& t)  { qty -= t.qty; }
};

int main() {
    print_line(1, 2.5, "three");
    std::tuple<int, double, long> t{1, 2.5, 3};
    double total = 0;
    for_each(t, [&](auto& x) { total += x; x *= 2; });
    std::printf("tuple total=%.1f, after doubling: %d %.1f %ld\n", total,
                std::get<0>(t), std::get<1>(t), std::get<2>(t));

    unsigned char body[16];
    Add a{7, 300};
    std::memcpy(body, &a, sizeof a);
    Counter c;
    const bool ok1 = dispatch<Add, Cancel, Trade>('A', body, c);
    const bool ok2 = dispatch<Add, Cancel, Trade>('Q', body, c);
    std::printf("dispatch A -> %d, Q -> %d; adds=%ld qty=%ld\n", ok1, ok2, c.adds, c.qty);
}
