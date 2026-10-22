// The same constraints written with enable_if, the detection idiom, and concepts (9.4).
#include <concepts>
#include <cstdio>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

// ---- C++11/14/17 style --------------------------------------------------------------
// (a) enable_if: if the condition is false, enable_if_t<...> has no 'type', substitution
//     fails, and the overload silently leaves the candidate set (SFINAE).
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
const char* kind_old(T) { return "integral (enable_if)"; }

template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
const char* kind_old(T) { return "floating (enable_if)"; }

// (b) detection idiom: does T have a .size() member? void_t maps any valid type to void.
template <class T, class = void>
struct has_size : std::false_type {};
template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>>
    : std::true_type {};
static_assert(has_size<std::string>::value && !has_size<int>::value);

// ---- C++20 style --------------------------------------------------------------------
template <class T>
concept Sized = requires(const T& t) {
    { t.size() } -> std::convertible_to<std::size_t>;   // expression must compile AND
};                                                      // its type must convert
template <class T>
concept RandomAccessSized = Sized<T> && requires(const T& t, std::size_t i) { t[i]; };

const char* kind(std::integral auto)       { return "integral"; }   // abbreviated form
const char* kind(std::signed_integral auto) { return "signed integral (more specific)"; }
const char* kind(std::floating_point auto) { return "floating point"; }
template <Sized T>             const char* kind(const T&) { return "sized"; }
template <RandomAccessSized T> const char* kind(const T&) { return "sized + indexable"; }

// requires-clause on a non-template member of a class template
template <class T>
struct Box {
    T v;
    auto size() const requires Sized<T> { return v.size(); }  // exists only if T is Sized
};

// Subsumption works only through NAMED concepts: two textually identical requires-
// expressions written in two places are unrelated atomic constraints (Problem P9.5).

int main() {
    std::printf("kind_old(1)   : %s\n", kind_old(1));
    std::printf("kind_old(1.0) : %s\n", kind_old(1.0));
    std::printf("kind(42u)     : %s\n", kind(42u));
    std::printf("kind(42)      : %s\n", kind(42));
    std::printf("kind(2.5f)    : %s\n", kind(2.5f));
    std::printf("kind(list)    : %s\n", kind(std::list<int>{1, 2}));
    std::printf("kind(vector)  : %s\n", kind(std::vector<int>{1, 2, 3}));
    Box<std::string> b{"abc"};
    Box<int> bi{7};                           // fine: size() simply does not exist
    std::printf("Box<string>::size() = %zu; Box<int> has size(): %s\n", b.size(),
                Sized<Box<int>> ? "yes" : "no");
    (void)bi;
}
