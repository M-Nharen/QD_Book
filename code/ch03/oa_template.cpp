// book: norun
// Online-assessment template (Section 3.2). It reads stdin, so the build only compiles it.
#include <bits/stdc++.h>
using namespace std;      // acceptable in a one-file OA answer; never in a header

using ll = long long;
using ull = unsigned long long;
constexpr ll MOD = 1'000'000'007LL;
constexpr ll INF = 2'000'000'000'000'000'000LL;   // INF + INF still fits in a long long

// ---- input: pull stdin in 64 KiB blocks with fread, parse integers by hand --------------
static char ibuf[1 << 16];
static size_t ipos = 0, ilen = 0;
inline int gc() {
    if (ipos == ilen) {
        ilen = fread(ibuf, 1, sizeof ibuf, stdin);
        ipos = 0;
        if (ilen == 0) return -1;                  // end of input
    }
    return ibuf[ipos++];
}
inline ll read_ll() {
    int c = gc();
    while (c != -1 && c != '-' && (c < '0' || c > '9')) c = gc();   // skip whitespace
    const bool neg = (c == '-');
    if (neg) c = gc();
    ll x = 0;
    for (; c >= '0' && c <= '9'; c = gc()) x = x * 10 + (c - '0');
    return neg ? -x : x;
}

// ---- output: append to one buffer, write it with a single fwrite at exit ----------------
static string obuf;
inline void write_ll(ll x) {
    char tmp[24];
    int len = 0;
    ull u = x < 0 ? 0ULL - ull(x) : ull(x);        // safe for LLONG_MIN, no signed overflow
    do { tmp[len++] = char('0' + u % 10); u /= 10; } while (u);
    if (x < 0) obuf.push_back('-');
    while (len) obuf.push_back(tmp[--len]);
}
inline void put(char c) { obuf.push_back(c); }

#ifdef LOCAL                                       // compile with -DLOCAL while practising
#define dbg(x) (cerr << __LINE__ << ": " #x " = " << (x) << '\n')
#else
#define dbg(x) ((void)0)
#endif

// ---- the problem: maximum sum of a non-empty subarray, T test cases ---------------------
void solve() {
    const int n = int(read_ll());
    vector<ll> a(n);
    for (ll& v : a) v = read_ll();
    ll best = LLONG_MIN, cur = 0;                  // sentinel below every possible answer
    for (ll v : a) {
        cur = max(cur + v, v);                     // best subarray ending exactly here
        best = max(best, cur);
    }
    dbg(best);
    write_ll(best);
    put('\n');
}

int main() {
    int t = int(read_ll());
    while (t-- > 0) solve();
    fwrite(obuf.data(), 1, obuf.size(), stdout);
}
