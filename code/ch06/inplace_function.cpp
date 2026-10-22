// Solution to P6.12: a type-erased callable with fixed inline storage and no heap.
// The "vtable" is written by hand: one static table of function pointers per stored type.
#include <cstddef>
#include <cstdio>
#include <new>
#include <type_traits>
#include <utility>

template <class Sig, std::size_t Cap = 32> class InplaceFunction;

template <class R, class... Args, std::size_t Cap>
class InplaceFunction<R(Args...), Cap> {
    struct Ops {                                  // the hand-made vtable
        R (*invoke)(void*, Args&&...);
        void (*copy)(const void* src, void* dst);
        void (*move)(void* src, void* dst) noexcept;
        void (*destroy)(void*) noexcept;
    };
    template <class F> static constexpr Ops ops_for{
        [](void* p, Args&&... a) -> R {
            return (*static_cast<F*>(p))(std::forward<Args>(a)...);
        },
        [](const void* s, void* d) { ::new (d) F(*static_cast<const F*>(s)); },
        [](void* s, void* d) noexcept { ::new (d) F(std::move(*static_cast<F*>(s))); },
        [](void* p) noexcept { static_cast<F*>(p)->~F(); },
    };

    alignas(std::max_align_t) unsigned char buf_[Cap];
    const Ops* ops_ = nullptr;                    // null means empty

public:
    InplaceFunction() = default;
    template <class F, class D = std::decay_t<F>,
              class = std::enable_if_t<!std::is_same_v<D, InplaceFunction>>>
    InplaceFunction(F&& f) {                      // NOLINT: implicit, like std::function
        static_assert(sizeof(D) <= Cap, "callable too large for the inline buffer");
        static_assert(alignof(D) <= alignof(std::max_align_t), "over-aligned callable");
        static_assert(std::is_nothrow_move_constructible_v<D>, "move must not throw");
        ::new (static_cast<void*>(buf_)) D(std::forward<F>(f));
        ops_ = &ops_for<D>;
    }
    InplaceFunction(const InplaceFunction& o) : ops_(o.ops_) {
        if (ops_) ops_->copy(o.buf_, buf_);
    }
    InplaceFunction(InplaceFunction&& o) noexcept : ops_(o.ops_) {
        if (ops_) { ops_->move(o.buf_, buf_); o.reset(); }
    }
    InplaceFunction& operator=(InplaceFunction o) noexcept {   // copy-and-swap via moves
        reset();
        if (o.ops_) { o.ops_->move(o.buf_, buf_); ops_ = o.ops_; o.reset(); }
        return *this;
    }
    ~InplaceFunction() { reset(); }
    void reset() noexcept {
        if (ops_) { ops_->destroy(buf_); ops_ = nullptr; }
    }
    explicit operator bool() const noexcept { return ops_ != nullptr; }
    R operator()(Args... a) { return ops_->invoke(buf_, std::forward<Args>(a)...); }
};

int main() {
    long fills = 0;
    InplaceFunction<long(long)> on_fill = [&fills](long q) { return fills += q; };
    InplaceFunction<long(long)> copy = on_fill;               // copies the lambda
    on_fill(100);
    copy(5);                                                   // same captured reference
    InplaceFunction<long(long)> moved = std::move(on_fill);
    std::printf("fills = %ld, moved(1) = %ld, moved-from empty: %s\n", fills, moved(1),
                on_fill ? "no" : "yes");
    std::printf("sizeof(InplaceFunction<long(long)>) = %zu\n", sizeof(moved));
}
