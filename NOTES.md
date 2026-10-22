# Running notes (consistency between turns)

## Final chapter order
1 Hiring · 2 Probability · 3 DSA I · 4 DSA II · 5 C++ Foundations Bridge · 6 Object Model ·
7 Memory Mgmt · 8 Modern C++ · 9 Compile-time · 10 STL internals · 11 UB · 12 Architecture ·
13 OS/Linux · 14 Concurrency · 15 Networking · 16 Compilers/Profiling · 17 Microstructure ·
18 Low-latency components · 19 Capstone · 20 Interview strategy · 21 PPO
Appendices: A Solutions · B Cheat sheets · C Toolchain · D Reading list

## Cross-references already promised (must exist when written)
- 3.1 complexity rule of thumb from input limits (Ch1 OA strategy)
- 3.2 OA C++ template with fast I/O, long long discipline (Ch1)
- 3.7 heaps; bottom-up heapify O(n) (diagnostic D1)
- 4.7 strings: KMP failure function, linear-time version of pattern_exact fallback (Ch2 2.7, 2.11)
- Ch6 struct padding (D4); Ch8 std::move is a cast (D7); Ch10 vector invalidation (D5);
  Ch11 signed overflow UB (D6); Ch12 64B lines (D8); Ch14 acquire/release (D10); Ch15 UDP multicast (D9)
- Ch 16 benchmarking (rdtsc etc.); Ch 19 capstone (résumé bullets in 1.3)

## Conventions
- Listings: code/chNN/*.cpp, <=92 cols, -Werror clean, referenced by line numbers in text.
- Benchmarks: label as measured in build container (virtualized Xeon 2.1 GHz, 2 vCPU).
- Practice problems: **Pn.k [E/M/H]**; solutions in solutions/chNN.md as "### *Pn.k".
- Chapter 2 generator xoshiro256** (Listing 2.2) can be reused later by reference.
