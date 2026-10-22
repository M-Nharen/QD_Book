// One template in the source, one function per argument list in the binary (Section 9.1).
#include <cstdio>
#include <string>
#include <vector>

template <class T>
T clamp_add(T a, T b, T hi) {
    std::puts(__PRETTY_FUNCTION__);          // shows which instantiation is running
    const T s = a + b;
    return s > hi ? hi : s;
}

// Explicit instantiation definition: emit clamp_add<double> in this TU even if unused.
template double clamp_add<double>(double, double, double);

template <class T>
struct Wrapper {
    T v;
    auto length() const { return v.size(); } // ill-formed for T = int ...
    T get() const { return v; }
};

template <class T>
struct Base {
    int grown = 0;
    void grow() { ++grown; }
};

template <class T>
struct Derived : Base<T> {
    std::size_t n = 0;
    void push() {
        typename std::vector<T>::size_type step = 1;  // dependent type: needs 'typename'
        this->grow();                         // member of a dependent base: needs 'this->'
        n += step;
    }
};

// A full specialization replaces the primary template for one argument list.
template <class T> struct Describe        { static constexpr const char* s = "generic"; };
template <>        struct Describe<bool>  { static constexpr const char* s = "bool"; };
// A partial specialization matches a family of argument lists.
template <class T> struct Describe<T*>    { static constexpr const char* s = "pointer"; };

int main() {
    clamp_add(1, 2, 10);                      // T deduced as int
    clamp_add<long>(1, 2, 10);                // T given explicitly: long
    clamp_add(1.5, 2.0, 3.0);                 // T deduced as double
    clamp_add<std::string>("ab", "cd", "zz"); // string: operator+ and operator>
    // clamp_add(1, 2.0, 3);                  // error: T deduced as int AND double

    Wrapper<int> w{42};                       // ... but fine: length() is never used,
    std::printf("w.get() = %d\n", w.get());   //     so it is never instantiated
    Wrapper<std::string> ws{"hello"};
    std::printf("ws.length() = %zu\n", ws.length());

    Derived<int> d;
    d.push(); d.push();
    std::printf("grown = %d, n = %zu\n", d.grown, d.n);

    std::printf("%s %s %s\n", Describe<int>::s, Describe<bool>::s, Describe<char*>::s);
}
