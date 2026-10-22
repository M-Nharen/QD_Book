// Adding an empty destructor silently turns every move into a copy (Section 5.5).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static long g_allocs = 0;                        // counts every global operator new call
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct QuoteZero {                               // rule of zero: declares nothing
    std::string symbol;
    std::vector<long> levels;
};

struct QuoteDtor {                               // same data + a "harmless" destructor
    std::string symbol;
    std::vector<long> levels;
    ~QuoteDtor() {}                              // user-declared: no implicit moves
};

struct QuoteFive {                               // destructor kept, moves restored
    std::string symbol;
    std::vector<long> levels;
    QuoteFive() = default;
    ~QuoteFive() {}
    QuoteFive(const QuoteFive&) = default;
    QuoteFive& operator=(const QuoteFive&) = default;
    QuoteFive(QuoteFive&&) noexcept = default;
    QuoteFive& operator=(QuoteFive&&) noexcept = default;
};

template <class Q>
void run(const char* name) {
    constexpr int N = 200'000;
    long allocs = 0;
    double best_ms = 1e9;
    for (int rep = 0; rep < 5; ++rep) {          // report the fastest of 5 runs
        const long before = g_allocs;
        const auto t0 = std::chrono::steady_clock::now();
        {
            std::vector<Q> book;                 // no reserve(): grows geometrically
            for (int i = 0; i < N; ++i) {
                Q q;
                q.symbol = "SYMBOL-LONGNAME-" + std::to_string(i);   // too long for SSO
                q.levels.assign(8, i);
                book.push_back(std::move(q));    // a move only if Q has a move ctor
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        allocs = g_allocs - before;
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        best_ms = ms < best_ms ? ms : best_ms;
    }
    std::printf("%-10s nothrow-movable: %-3s  allocations: %7ld  best time: %5.1f ms\n",
                name, std::is_nothrow_move_constructible_v<Q> ? "yes" : "no", allocs,
                best_ms);
}

int main() {
    run<QuoteZero>("QuoteZero");
    run<QuoteDtor>("QuoteDtor");
    run<QuoteFive>("QuoteFive");
}
