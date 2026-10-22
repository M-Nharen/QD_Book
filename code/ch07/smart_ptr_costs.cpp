// What smart pointers really cost: size, allocations, control-block lifetime and
// reference-count traffic (Section 7.6).
// book: flags=-pthread
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <thread>

int g_news = 0;
bool g_log = false;
void* operator new(std::size_t n) {
    ++g_news;
    if (g_log) std::printf("    operator new(%zu)\n", n);
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t n) noexcept {
    if (g_log) std::printf("    operator delete(%zu)\n", n);
    std::free(p);
}

struct Order {
    long id = 0, price = 0, qty = 0;
    ~Order() { if (g_log) std::printf("    ~Order()\n"); }
};
struct FreeDeleter { void operator()(int* p) const noexcept { std::free(p); } };

int allocations(auto make) { int before = g_news; auto p = make(); return g_news - before; }

// noipa: the optimizer may not inline these or reason about their bodies.
[[gnu::noipa]] long by_value(std::shared_ptr<Order> p) { return p->qty; }
[[gnu::noipa]] long by_cref(const std::shared_ptr<Order>& p) { return p->qty; }

template <class F> double ns_per_call(F f) {
    constexpr int N = 20'000'000;
    long sum = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) sum += f();
    auto t1 = std::chrono::steady_clock::now();
    if (sum == 42) std::printf(" ");               // keep sum alive
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
}

void report(const std::shared_ptr<Order>& order) {
    const double v = ns_per_call([&] { return by_value(order); });
    const double r = ns_per_call([&] { return by_cref(order); });
    std::printf("  by value %.2f   by const& %.2f\n", v, r);
}

int main() {
    std::printf("Sizes (bytes):\n  Order*              %zu\n", sizeof(Order*));
    std::printf("  unique_ptr<Order>   %zu\n", sizeof(std::unique_ptr<Order>));
    std::printf("  unique_ptr<int, FreeDeleter>        %zu  (empty deleter: free)\n",
                sizeof(std::unique_ptr<int, FreeDeleter>));
    std::printf("  unique_ptr<int, void(*)(int*)>      %zu  (stores a function ptr)\n",
                sizeof(std::unique_ptr<int, void (*)(int*)>));
    std::printf("  unique_ptr<int, function<void(int*)>> %zu\n",
                sizeof(std::unique_ptr<int, std::function<void(int*)>>));
    std::printf("  shared_ptr<Order>   %zu   weak_ptr<Order> %zu\n",
                sizeof(std::shared_ptr<Order>), sizeof(std::weak_ptr<Order>));

    std::printf("Heap allocations per object:\n");
    std::printf("  make_unique<Order>()              %d\n",
                allocations([] { return std::make_unique<Order>(); }));
    std::printf("  shared_ptr<Order>(new Order)      %d\n",
                allocations([] { return std::shared_ptr<Order>(new Order); }));
    std::printf("  make_shared<Order>()              %d\n",
                allocations([] { return std::make_shared<Order>(); }));

    std::printf("make_shared + weak_ptr: object destroyed early, memory freed late\n");
    g_log = true;
    auto sp = std::make_shared<Order>();
    std::weak_ptr<Order> wp = sp;
    std::printf("  reset last shared_ptr:\n");
    sp.reset();
    std::printf("  reset last weak_ptr:\n");
    wp.reset();
    g_log = false;

    auto order = std::make_shared<Order>();
    order->qty = 1;
    std::printf("Passing shared_ptr (ns per call, one thread running):\n");
    report(order);
    std::thread([] {}).join();                     // the process is now multi-threaded
    std::printf("Same calls after a second thread has ever existed:\n");
    report(order);
}
