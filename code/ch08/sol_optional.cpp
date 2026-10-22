// Solution P8.10: a minimal Optional<T> that manages the lifetime of its value by hand,
// tested by counting live objects through every engaged/empty combination.
#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

template <class T> class Optional {
    union { T value_; };                           // no default member: not constructed
    bool engaged_ = false;
public:
    Optional() noexcept {}
    Optional(const T& v) : value_(v), engaged_(true) {}
    Optional(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(v)), engaged_(true) {}
    Optional(const Optional& o) {
        if (o.engaged_) {
            std::construct_at(std::addressof(value_), o.value_);
            engaged_ = true;
        }
    }
    Optional(Optional&& o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (o.engaged_) {
            std::construct_at(std::addressof(value_), std::move(o.value_));
            engaged_ = true;                       // o stays engaged, holding a
        }                                          // moved-from T, as std::optional does
    }
    Optional& operator=(const Optional& o) {
        if (this == &o) return *this;
        if (engaged_ && o.engaged_) value_ = o.value_;     // both full: assign
        else if (o.engaged_) emplace(o.value_);            // empty <- full: construct
        else reset();                                      // full or empty <- empty
        return *this;
    }
    Optional& operator=(Optional&& o) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                               std::is_nothrow_move_assignable_v<T>) {
        if (this == &o) return *this;
        if (engaged_ && o.engaged_) value_ = std::move(o.value_);
        else if (o.engaged_) emplace(std::move(o.value_));
        else reset();
        return *this;
    }
    ~Optional() { reset(); }

    template <class... A> T& emplace(A&&... args) {
        reset();
        std::construct_at(std::addressof(value_), std::forward<A>(args)...);
        engaged_ = true;                           // set only after construction succeeded
        return value_;
    }
    void reset() noexcept {
        if (engaged_) { std::destroy_at(std::addressof(value_)); engaged_ = false; }
    }
    bool has_value() const noexcept { return engaged_; }
    T& operator*() noexcept { return value_; }     // unchecked, like std::optional
    template <class U> T value_or(U&& d) const& {
        return engaged_ ? value_ : static_cast<T>(std::forward<U>(d));
    }
};

int live = 0, built = 0;
struct Life {
    std::string s;
    explicit Life(std::string v) : s(std::move(v)) { ++live; ++built; }
    Life(const Life& o) : s(o.s) { ++live; ++built; }
    Life(Life&& o) noexcept : s(std::move(o.s)) { ++live; ++built; }
    Life& operator=(const Life&) = default;
    Life& operator=(Life&&) noexcept = default;
    ~Life() { --live; }
};

int main() {
    {
        Optional<Life> a, b(Life("bid")), c(Life("ask"));
        std::printf("after construction: live %d\n", live);   // b, c (temporaries gone)
        a = b;                                     // empty <- full: copy-construct
        b = Optional<Life>();                      // full <- empty: destroy
        c = a;                                     // full <- full: assign
        Optional<Life> d(std::move(c));            // move-construct; c keeps a husk
        a.emplace("last");                         // destroy old, construct new
        std::printf("a=%s b.has_value=%d c.has_value=%d d=%s live %d\n",
                    (*a).s.c_str(), int(b.has_value()), int(c.has_value()),
                    (*d).s.c_str(), live);
        std::printf("b.value_or: %s\n", b.value_or(Life("none")).s.c_str());
    }
    std::printf("after scope: live %d (must be 0), objects ever built %d\n", live, built);
    return live == 0 ? 0 : 1;
}
