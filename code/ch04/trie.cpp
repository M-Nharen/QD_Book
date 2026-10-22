// Tries stored in one array of nodes (indices, not pointers): words and binary XOR (4.1).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

struct WordTrie {                                  // lowercase words; prefix counting
    struct Node { std::array<int, 26> next; int pass = 0, end = 0; };
    std::vector<Node> t;
    WordTrie() { t.push_back(Node{}); t[0].next.fill(-1); }
    void insert(const std::string& w) {
        int v = 0;
        for (char ch : w) {
            const int c = ch - 'a';
            if (t[v].next[c] < 0) {                // new node: append to the pool
                t[v].next[c] = int(t.size());
                t.push_back(Node{});
                t.back().next.fill(-1);
            }
            v = t[v].next[c];
            ++t[v].pass;                           // words passing through this node
        }
        ++t[v].end;
    }
    int count_prefix(const std::string& p) const {
        int v = 0;
        for (char ch : p)
            if ((v = t[v].next[ch - 'a']) < 0) return 0;
        return t[v].pass;
    }
};

struct XorTrie {                                   // 31-bit non-negative integers
    static constexpr int B = 30;
    std::vector<std::array<int, 2>> ch{{-1, -1}};  // node 0 is the root
    void insert(std::uint32_t x) {
        int v = 0;
        for (int b = B; b >= 0; --b) {
            const int bit = int((x >> b) & 1u);
            if (ch[v][bit] < 0) { ch[v][bit] = int(ch.size()); ch.push_back({-1, -1}); }
            v = ch[v][bit];
        }
    }
    std::uint32_t max_xor_with(std::uint32_t x) const {    // trie must be non-empty
        int v = 0;
        std::uint32_t r = 0;
        for (int b = B; b >= 0; --b) {             // greedily prefer the opposite bit
            const int want = int(((x >> b) & 1u) ^ 1u);
            if (ch[v][want] >= 0) { r |= 1u << b; v = ch[v][want]; }
            else v = ch[v][want ^ 1];
        }
        return r;
    }
};

std::uint32_t max_xor_pair(const std::vector<std::uint32_t>& a) {   // O(n * 31)
    XorTrie t;
    std::uint32_t best = 0;
    t.insert(a[0]);
    for (std::size_t i = 1; i < a.size(); ++i) {
        best = std::max(best, t.max_xor_with(a[i]));
        t.insert(a[i]);
    }
    return best;
}

int main() {
    WordTrie w;
    for (const char* s : {"buy", "bid", "bidder", "book", "booking", "sell"}) w.insert(s);
    std::printf("prefix 'bid': %d, 'boo': %d, 'b': %d, 'x': %d, nodes %zu\n",
                w.count_prefix("bid"), w.count_prefix("boo"), w.count_prefix("b"),
                w.count_prefix("x"), w.t.size());

    std::printf("max xor of {3,10,5,25,2,8} = %u\n", max_xor_pair({3, 10, 5, 25, 2, 8}));
    std::mt19937 rng(9);
    int bad = 0;
    for (int t = 0; t < 300; ++t) {                // against the O(n^2) definition
        std::vector<std::uint32_t> a(2 + rng() % 40);
        for (auto& x : a) x = rng() >> 1;
        std::uint32_t b = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = i + 1; j < a.size(); ++j) b = std::max(b, a[i] ^ a[j]);
        bad += max_xor_pair(a) != b;
    }
    std::printf("random cross-check: %d mismatches in 300\n", bad);
}
