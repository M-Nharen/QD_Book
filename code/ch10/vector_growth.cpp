// std::vector growth, the noexcept-move rule, reserve and invalidation (Section 10.1).
#include <cstdio>
#include <utility>
#include <vector>

struct Counts { long copies = 0, moves = 0; };

template <bool NoexceptMove>
struct Tracked {
    static inline Counts c;
    int v = 0;
    explicit Tracked(int x) : v(x) {}
    Tracked(const Tracked& o) : v(o.v) { ++c.copies; }
    Tracked(Tracked&& o) noexcept(NoexceptMove) : v(o.v) { ++c.moves; }
    Tracked& operator=(const Tracked& o) { v = o.v; ++c.copies; return *this; }
    Tracked& operator=(Tracked&& o) noexcept(NoexceptMove) { v = o.v; ++c.moves;
                                                             return *this; }
};

template <class T>
void grow(const char* name, int n, bool reserve_first, bool show_capacities) {
    T::c = {};
    std::vector<T> v;
    if (reserve_first) v.reserve(n);
    int reallocations = 0;
    if (show_capacities) std::printf("capacities:");
    for (int i = 0; i < n; ++i) {
        const auto cap = v.capacity();
        v.emplace_back(i);                    // constructs in place: no copy, no move
        if (v.capacity() != cap) {            // the buffer was replaced
            if (cap != 0) ++reallocations;
            if (show_capacities) std::printf(" %zu", v.capacity());
        }
    }
    if (show_capacities) std::printf("\n");
    std::printf("%-26s reallocations=%2d copies=%4ld moves=%4ld\n", name, reallocations,
                T::c.copies, T::c.moves);
}

int main() {
    grow<Tracked<true>>("noexcept move", 1000, false, true);
    grow<Tracked<false>>("potentially-throwing move", 1000, false, false);
    grow<Tracked<true>>("noexcept move + reserve", 1000, true, false);

    // Inserting at the front shifts every element: n moves (assignments) per insert.
    Tracked<true>::c = {};
    std::vector<Tracked<true>> v;
    v.reserve(1001);
    for (int i = 0; i < 1000; ++i) v.emplace_back(i);
    Tracked<true>::c = {};
    v.insert(v.begin(), Tracked<true>{-1});
    std::printf("insert at front of 1000:     moves=%ld copies=%ld\n",
                Tracked<true>::c.moves, Tracked<true>::c.copies);

    // clear() keeps the buffer; shrink_to_fit() is a non-binding request to release it.
    v.clear();
    std::printf("after clear: size=%zu capacity=%zu\n", v.size(), v.capacity());
    v.shrink_to_fit();
    std::printf("after shrink_to_fit: capacity=%zu\n", v.capacity());

    // vector<bool> is a packed bitset, not a container of bool.
    std::vector<bool> bits(1000);
    std::printf("vector<bool>(1000) capacity=%zu bits; operator[] returns a proxy\n",
                bits.capacity());
}
