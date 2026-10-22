// Solution P8.11: a perfect-forwarding wrapper that times any call and returns exactly
// what the callee returns: values, references and void.
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

long g_last_ns = 0;

template <class F, class... Args>
decltype(auto) timed_call(F&& f, Args&&... args) {
    const auto t0 = std::chrono::steady_clock::now();
    struct Stop {                                  // RAII: records the time on every exit,
        std::chrono::steady_clock::time_point t0;  // including when f throws
        ~Stop() {
            g_last_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        }
    } stop{t0};
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

int& slot(std::vector<int>& v, std::size_t i) { return v[i]; }
std::string make(int n) { return std::string(std::size_t(n), 'x'); }
void log_it(const std::string&) {}
struct Book { long best_bid(int) const { return 10137; } };

int main() {
    std::vector<int> v{1, 2, 3};
    timed_call(slot, v, 1) = 42;                   // returns int&: assignable
    static_assert(std::is_same_v<decltype(timed_call(slot, v, 1)), int&>);
    static_assert(std::is_same_v<decltype(timed_call(make, 3)), std::string>);
    static_assert(std::is_void_v<decltype(timed_call(log_it, std::string("x")))>);
    const std::string s = timed_call(make, 5);     // prvalue: no extra move (elided)
    timed_call(log_it, s);
    Book b;
    const long px = timed_call(&Book::best_bid, b, 0);   // member function via invoke
    std::printf("v[1] = %d, s = %s, px = %ld, duration recorded: %s\n", v[1], s.c_str(),
                px, g_last_ns >= 0 ? "yes" : "no");
}
