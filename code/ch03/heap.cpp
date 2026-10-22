// Binary min-heap in an array; bottom-up heapify is O(n), repeated insertion is not (3.7).
#include <bit>
#include <chrono>
#include <cstdio>
#include <utility>
#include <vector>

struct MinHeap {
    std::vector<int> a;
    long long moves = 0;                           // swaps performed, to compare methods

    void sift_up(std::size_t i) {
        while (i > 0) {
            const std::size_t p = (i - 1) / 2;     // parent
            if (a[p] <= a[i]) break;
            std::swap(a[p], a[i]);
            ++moves;
            i = p;
        }
    }
    void sift_down(std::size_t i) {
        const std::size_t n = a.size();
        for (;;) {
            std::size_t c = 2 * i + 1;             // left child; right child is c + 1
            if (c >= n) break;
            if (c + 1 < n && a[c + 1] < a[c]) ++c; // pick the smaller child
            if (a[i] <= a[c]) break;
            std::swap(a[i], a[c]);
            ++moves;
            i = c;
        }
    }
    void push(int x) { a.push_back(x); sift_up(a.size() - 1); }
    int pop() {                                    // precondition: not empty
        const int top = a[0];
        a[0] = a.back();
        a.pop_back();
        if (!a.empty()) sift_down(0);
        return top;
    }
    void build(std::vector<int> v) {               // bottom-up heapify (Floyd's method)
        a = std::move(v);
        for (std::size_t i = a.size() / 2; i-- > 0;) sift_down(i);   // last parent first
    }
};

int main() {
    for (int n : {1023, 1'000'000, (1 << 20) - 1}) {
        std::vector<int> desc(n);                  // descending input: worst case for both
        for (int i = 0; i < n; ++i) desc[i] = n - i;

        MinHeap h1, h2;
        auto t0 = std::chrono::steady_clock::now();
        h1.build(desc);
        auto t1 = std::chrono::steady_clock::now();
        for (int x : desc) h2.push(x);
        auto t2 = std::chrono::steady_clock::now();
        using ms = std::chrono::duration<double, std::milli>;
        std::printf("n=%8d  bottom-up: %9lld swaps (n - popcount(n) = %8d) %6.2f ms\n",
                    n, h1.moves, n - std::popcount(unsigned(n)), ms(t1 - t0).count());
        std::printf("%10s  insertion: %9lld swaps %33.2f ms\n", "",
                    h2.moves, ms(t2 - t1).count());

        bool sorted = true;                        // heap order check: pops come out sorted
        for (int prev = 0, i = 0; i < n; ++i) {
            const int x = h1.pop();
            sorted = sorted && x >= prev;
            prev = x;
        }
        if (!sorted) std::printf("BUG: pops not sorted\n");
    }
}
