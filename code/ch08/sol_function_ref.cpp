// Solution P8.9: FunctionRef, a non-owning reference to any callable. Two pointers, never
// allocates, one indirect call. The callable must outlive the FunctionRef.
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

int g_news = 0;
void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <class Sig> class FunctionRef;
template <class R, class... Args> class FunctionRef<R(Args...)> {
    void* obj_;
    R (*call_)(void*, Args...);
public:
    template <class F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> &&
                 std::is_invocable_r_v<R, F&, Args...>)
    FunctionRef(F&& f) noexcept                    // implicit on purpose
        : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
          call_([](void* o, Args... a) -> R {
              return std::invoke(*static_cast<std::remove_reference_t<F>*>(o),
                                 std::forward<Args>(a)...);
          }) {}
    R operator()(Args... a) const { return call_(obj_, std::forward<Args>(a)...); }
};

// A function that accepts any callback without being a template and without allocating.
long for_each_fill(const long* qty, int n, FunctionRef<void(long)> on_fill) {
    long total = 0;
    for (int i = 0; i < n; ++i) {
        on_fill(qty[i]);
        total += qty[i];
    }
    return total;
}

int main() {
    const long fills[] = {100, 250, 50};
    long position = 0, count = 0;
    struct Big { long pad[8]; long* pos; } big{{}, &position};   // 72-byte capture
    const int before = g_news;
    for_each_fill(fills, 3, [&](long q) { position += q; ++count; });
    for_each_fill(fills, 3, [big](long q) { *big.pos -= q / 2; });
    std::printf("position %ld after %ld fills; operator new calls: %d; sizeof %zu\n",
                position, count, g_news - before, sizeof(FunctionRef<void(long)>));
}
