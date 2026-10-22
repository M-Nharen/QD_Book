// Type traits by hand, std::conditional, and if constexpr in a serializer (Section 9.3).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>

// 1. A trait is a template whose specializations encode a fact about a type.
template <class T> struct is_ptr      : std::false_type {};
template <class T> struct is_ptr<T*>  : std::true_type {};
template <class T> inline constexpr bool is_ptr_v = is_ptr<T>::value;
static_assert(is_ptr_v<int*> && !is_ptr_v<int> && !is_ptr_v<int* const>); // cv matters!

template <class T> struct remove_ref      { using type = T; };
template <class T> struct remove_ref<T&>  { using type = T; };
template <class T> struct remove_ref<T&&> { using type = T; };
static_assert(std::is_same_v<remove_ref<const int&>::type, const int>);

// 2. Choosing a type at compile time: the smallest unsigned type that can hold N.
template <std::uint64_t N>
using uint_for = std::conditional_t<(N <= 0xFF), std::uint8_t,
                 std::conditional_t<(N <= 0xFFFF), std::uint16_t,
                 std::conditional_t<(N <= 0xFFFF'FFFF), std::uint32_t, std::uint64_t>>>;
static_assert(sizeof(uint_for<200>) == 1 && sizeof(uint_for<70'000>) == 4);

// 3. if constexpr: the branch not taken is discarded and never instantiated.
template <class> inline constexpr bool always_false = false;

struct Quote { std::int64_t px; std::int32_t qty; std::uint8_t side; };  // 16 bytes

template <class T>
std::size_t put(std::byte* out, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        out[0] = std::byte{v ? std::uint8_t{1} : std::uint8_t{0}};
        return 1;
    } else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
        std::memcpy(out, &v, sizeof v);       // one mov for an 8-byte value at -O2
        return sizeof v;
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        const auto n = static_cast<std::uint16_t>(v.size());
        std::memcpy(out, &n, 2);
        std::memcpy(out + 2, v.data(), n);    // a plain 'if' would also compile this
        return 2 + std::size_t{n};            // line for int, and fail: int has no .data()
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(out, &v, sizeof v);       // copies padding bytes too (Section 6.1)
        return sizeof v;
    } else {
        static_assert(always_false<T>, "no serializer for this type");
        return 0;
    }
}

template <class... Ts>
std::size_t put_all(std::byte* out, const Ts&... vs) {
    std::size_t n = 0;
    ((n += put(out + n, vs)), ...);           // fold expression, Section 9.6
    return n;
}

int main() {
    std::byte buf[64]{};
    const std::size_t n = put_all(buf, true, std::int32_t{-7}, 101.25,
                                  std::string_view{"XNSE"}, Quote{1'234'500, 300, 'B'});
    std::printf("serialized %zu bytes\nfirst 19:", n);  // 1 + 4 + 8 + (2 + 4) + 16
    for (std::size_t i = 0; i < 19; ++i) std::printf(" %02x", unsigned(buf[i]));
    // (the last 16 bytes are the raw Quote, whose 3 padding bytes are unspecified)
    std::printf("\nsizeof(uint_for<200>)=%zu  sizeof(uint_for<70000>)=%zu\n",
                sizeof(uint_for<200>), sizeof(uint_for<70'000>));
}
