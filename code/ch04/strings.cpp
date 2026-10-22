// Prefix function (KMP), Z-function, polynomial rolling hash, pattern waiting times (4.7).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
using u64 = std::uint64_t;

// pi[i] = length of the longest proper border (prefix that is also a suffix) of s[0..i].
std::vector<int> prefix_function(const std::string& s) {
    std::vector<int> pi(s.size(), 0);
    for (std::size_t i = 1; i < s.size(); ++i) {
        int k = pi[i - 1];                         // best border of the previous prefix
        while (k > 0 && s[i] != s[k]) k = pi[k - 1];   // fall back to shorter borders
        if (s[i] == s[k]) ++k;
        pi[i] = k;
    }
    return pi;
}

std::vector<int> kmp_find(const std::string& text, const std::string& pat) {
    const std::vector<int> pi = prefix_function(pat);
    std::vector<int> hits;
    int k = 0;                                     // characters of pat currently matched
    for (int i = 0; i < int(text.size()); ++i) {
        while (k > 0 && text[i] != pat[k]) k = pi[k - 1];
        if (text[i] == pat[k]) ++k;
        if (k == int(pat.size())) {                // full match ending at i
            hits.push_back(i - k + 1);
            k = pi[k - 1];                         // continue: matches may overlap
        }
    }
    return hits;
}

// z[i] = length of the longest common prefix of s and s[i..]; z[0] = |s| by convention.
std::vector<int> z_function(const std::string& s) {
    const int n = int(s.size());
    std::vector<int> z(n, 0);
    if (n) z[0] = n;
    for (int i = 1, l = 0, r = 0; i < n; ++i) {    // [l, r) is the rightmost known match
        if (i < r) z[i] = std::min(r - i, z[i - l]);
        while (i + z[i] < n && s[z[i]] == s[i + z[i]]) ++z[i];
        if (i + z[i] > r) { l = i; r = i + z[i]; }
    }
    return z;
}

// Polynomial hash modulo the prime 2^61 - 1 with a random base chosen at run time.
struct RollingHash {
    static constexpr u64 MOD = (1ULL << 61) - 1;
    static u64 mul(u64 a, u64 b) {                 // (a * b) mod 2^61-1 without division
        const unsigned __int128 p = (unsigned __int128)a * b;
        u64 r = u64(p & MOD) + u64(p >> 61);
        return r >= MOD ? r - MOD : r;
    }
    std::vector<u64> h, pw;                        // prefix hashes, powers of the base
    RollingHash(const std::string& s, u64 base) : h(s.size() + 1, 0), pw(s.size() + 1, 1) {
        for (std::size_t i = 0; i < s.size(); ++i) {
            h[i + 1] = (mul(h[i], base) + (unsigned char)s[i] + 1) % MOD;
            pw[i + 1] = mul(pw[i], base);
        }
    }
    u64 get(std::size_t l, std::size_t r) const { // hash of s[l..r)
        return (h[r] + MOD - mul(h[l], pw[r - l])) % MOD;
    }
};

// Expected number of i.i.d. uniform symbols (alphabet size m) until `pat` first appears:
// the sum of m^b over every border length b of pat, including b = |pat| (Section 2.7).
double expected_wait(const std::string& pat, int m) {
    const std::vector<int> pi = prefix_function(pat);
    double e = 0, mb = 1;
    std::vector<double> powm(pat.size() + 1, 1);
    for (std::size_t i = 1; i <= pat.size(); ++i) powm[i] = (mb *= m);
    for (int b = int(pat.size()); b > 0; b = pi[b - 1]) e += powm[b];   // walk the borders
    return e;
}

int main() {
    std::printf("pi(\"aabaaab\") =");
    for (int x : prefix_function("aabaaab")) std::printf(" %d", x);
    std::printf("\nKMP \"aba\" in \"abababa\": at");
    for (int x : kmp_find("abababa", "aba")) std::printf(" %d", x);
    std::printf("\nz(\"aabxaab\") =");
    for (int x : z_function("aabxaab")) std::printf(" %d", x);
    std::printf("\nexpected flips:");
    for (const char* p : {"HH", "HT", "HTH", "HHT", "HTHT", "HHHHH"})
        std::printf(" %s=%g", p, expected_wait(p, 2));
    std::printf("\n");

    std::mt19937_64 rng(u64(std::chrono::steady_clock::now().time_since_epoch().count()));
    const u64 base = 1'000'003 + rng() % (RollingHash::MOD - 2'000'000);   // random base
    std::mt19937 small(13);                        // fixed seed for the test data
    int bad = 0;
    for (int t = 0; t < 300; ++t) {
        std::string s(1 + small() % 60, 'a'), p(1 + small() % 4, 'a');
        for (char& c : s) c = char('a' + small() % 2);   // binary alphabet: many repeats
        for (char& c : p) c = char('a' + small() % 2);
        std::vector<int> naive;
        for (std::size_t i = 0; i + p.size() <= s.size(); ++i)
            if (s.compare(i, p.size(), p) == 0) naive.push_back(int(i));
        bad += kmp_find(s, p) != naive;
        const std::vector<int> z = z_function(s);
        for (std::size_t i = 1; i < s.size(); ++i) {
            std::size_t k = 0;
            while (i + k < s.size() && s[k] == s[i + k]) ++k;
            bad += z[i] != int(k);
        }
        const RollingHash rh(s, base);
        for (int q = 0; q < 50; ++q) {             // equal substrings <=> equal hashes
            const std::size_t len = 1 + small() % s.size();
            const std::size_t a = small() % (s.size() - len + 1);
            const std::size_t b = small() % (s.size() - len + 1);
            bad += (rh.get(a, a + len) == rh.get(b, b + len)) !=
                   (s.compare(a, len, s, b, len) == 0);
        }
    }
    std::printf("random cross-check: %d mismatches\n", bad);
}
