// Lambdas are compiler-generated classes; std::function is a type-erasing owner of any
// callable (Section 8.7).
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

struct Strategy {
    long position = 0;
    auto on_fill_callback() {                      // [this] captures a pointer only:
        return [this](long qty) { position += qty; };  // the object must outlive it
    }
};

int apply(int (*fp)(int), int v) { return fp(v); }

int main() {
    int x = 1, y = 2;
    std::string venue(40, 'v');
    auto none = [] { return 0; };
    auto by_val = [x] { return x; };
    auto by_ref = [&x] { return x; };
    auto both = [x, &y] { return x + y; };
    auto init = [s = std::move(venue)] { return s.size(); };   // init-capture: moved in
    auto generic = [](auto a, auto b) { return a + b; };        // template operator()
    std::printf("sizeof closure: [] %zu, [x] %zu, [&x] %zu, [x,&y] %zu, "
                "[s=std::move(str)] %zu, generic %zu\n", sizeof none, sizeof by_val,
                sizeof by_ref, sizeof both, sizeof init, sizeof generic);
    std::printf("generic(1, 2) = %d, generic(1.5, 2.25) = %.2f\n", generic(1, 2),
                generic(1.5, 2.25));
    std::printf("two identical lambdas have the same type? %s\n",
                std::is_same_v<decltype([] {}), decltype([] {})> ? "yes" : "no");

    // Capture by value happens once, when the closure is created.
    int limit = 100;
    auto check = [limit](int q) { return q <= limit; };
    limit = 5;
    std::printf("captured limit still 100: check(50) = %d\n", int(check(50)));

    // mutable: the closure's copy of the capture may change between calls.
    auto counter = [n = 0]() mutable { return ++n; };
    counter();
    std::printf("mutable counter after two calls: %d\n", counter());

    // A captureless lambda converts to a plain function pointer.
    const int doubled = apply(+[](int v) { return v * 2; }, 20);   // unary + converts
    std::printf("apply(+[](int v){...}, 20) = %d\n", doubled);

    Strategy s;
    auto fill = s.on_fill_callback();
    fill(100);
    fill(-30);
    std::printf("position via [this] callback: %ld\n", s.position);

    std::function<long()> f = [x, y] { return long(x) * y; };
    std::printf("sizeof(std::function<long()>) = %zu; f() = %ld; empty f throws "
                "bad_function_call\n", sizeof f, f());
    try {
        std::function<void()> empty;
        empty();
    } catch (const std::bad_function_call&) {
        std::printf("caught std::bad_function_call\n");
    }
}
