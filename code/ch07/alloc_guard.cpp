// Enforcing "no allocation on the hot path": a thread-local guard inside a replaced
// global operator new, used as a test harness to find hidden allocations (Section 7.7).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace hot {
thread_local int depth = 0;                        // > 0: we are on the hot path
thread_local long count = 0;                       // allocations seen while hot
}

void* acquire(std::size_t n) noexcept {
    if (hot::depth > 0) ++hot::count;              // production: log + abort() here
    return std::malloc(n ? n : 1);
}
// noinline: stops GCC's -Wmismatched-new-delete false positive on free() (Listing 7.3)
[[gnu::noinline]] void release(void* p) noexcept { std::free(p); }

void* operator new(std::size_t n) {
    if (void* p = acquire(n)) return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return acquire(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }

struct HotScope {                                  // RAII marker for the hot region
    HotScope() { ++hot::depth; }
    ~HotScope() { --hot::depth; }
    HotScope(const HotScope&) = delete;
    HotScope& operator=(const HotScope&) = delete;
};

template <class F> void check(const char* what, F&& f) {
    const long before = hot::count;
    {
        HotScope s;
        f();
    }
    std::printf("  %-50s %ld\n", what, hot::count - before);
}

struct Pod24 { long a, b, c; };

int main() {
    std::vector<int> reserved;
    reserved.reserve(1000);
    std::vector<int> unreserved;
    std::unordered_map<int, int> book{{1, 10}};
    std::map<int, int> levels{{1, 10}};
    std::vector<int> data(1000);
    for (int i = 0; i < 1000; ++i) data[i] = (i * 7919) % 1000;
    char out[64];

    std::printf("operator new calls inside each hot region:\n");
    check("std::string, 15 chars (fits SSO)", [] { std::string s("123456789012345"); });
    check("std::string, 16 chars", [] { std::string s("1234567890123456"); });
    check("std::to_string(42)", [] { auto s = std::to_string(42); });
    check("100 push_back into reserved vector", [&] {
        for (int i = 0; i < 100; ++i) reserved.push_back(i);
    });
    check("100 push_back into empty vector", [&] {
        for (int i = 0; i < 100; ++i) unreserved.push_back(i);
    });
    check("std::function, lambda capturing 16 bytes", [] {
        long a = 1, b = 2;
        std::function<long()> f = [a, b] { return a + b; };
    });
    check("std::function, lambda capturing 24 bytes", [] {
        Pod24 p{1, 2, 3};
        std::function<long()> f = [p] { return p.a + p.c; };
    });
    check("unordered_map lookup of existing key", [&] { (void)book.find(1); });
    check("unordered_map insert of new key", [&] { book.emplace(2, 20); });
    check("map operator[] on existing key", [&] { levels[1] += 1; });
    check("std::sort of 1000 ints", [&] { std::sort(data.begin(), data.end()); });
    check("std::stable_sort of 1000 ints", [&] {
        std::stable_sort(data.begin(), data.end());
    });
    check("std::format_to_n into a char buffer", [&] {
        std::format_to_n(out, sizeof out - 1, "{} @ {}", 100, 9950);
    });
    check("std::ostringstream << int", [] {
        std::ostringstream os;
        os << 42;
    });
    check("make_shared<int>", [] { auto p = std::make_shared<int>(1); });
    check("throw/catch std::runtime_error (message only)", [] {
        try { throw std::runtime_error("x"); } catch (const std::exception&) {}
    });
    check("throw/catch int", [] {
        try { throw 1; } catch (int) {}
    });
    std::printf("(the exception object itself comes from __cxa_allocate_exception, which "
                "calls\n malloc directly, so this hook cannot see it: see Section 7.7)\n");
}
