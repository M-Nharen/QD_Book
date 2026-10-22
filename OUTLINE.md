# Canonical outline (section numbers are FIXED — cross-reference these freely)

Every technical chapter = its content sections below, followed by these closing sections
(numbered in sequence after the content sections): Code Showdown (if not already inside a
content section) · The Nanosecond Penalty · Interview Questions · Practice Problems · Key Takeaways.
You may add ### subsections and extra closing material, but do NOT renumber, reorder or
remove the ## content sections listed here, because other chapters cite them.

## Part A — Getting through the screen   (#! line goes at top of ch01.md — DONE)
1  How Quant Developer Hiring Works — DONE
2  Probability and Brainteasers — DONE (2.1 How tested · 2.2 Counting · 2.3 Bayes · 2.4 Expectation
   · 2.5 Distributions · 2.6 Continuous/geometric · 2.7 Markov chains · 2.8 Random walks/martingales
   · 2.9 Puzzles · 2.10 Mental math · 2.11 Code showdown · 2.12 Nanosecond penalty · 2.13 Interview Qs
   · 2.14 Practice · 2.15 Key takeaways)
3  DSA I: Foundations for Online Tests
   3.1 Complexity and Reading the Constraints (ops/sec rule of thumb, limit→complexity table, amortized)
   3.2 C++ for Online Tests (template, fast I/O, overflow, long long, bits/stdc++ trade-offs, debugging)
   3.3 Arrays, Prefix Sums, Difference Arrays and Hashing (anti-hash tests, custom hash)
   3.4 Two Pointers and Sliding Window
   3.5 Binary Search and Binary Search on the Answer (invariant-based template)
   3.6 Stacks, Queues, Monotonic Stack and Deque
   3.7 Heaps and Priority Queues (bottom-up heapify is O(n) — cited by diagnostic D1)
   3.8 Bit Manipulation
   3.9 Greedy Algorithms and Exchange Arguments
4  DSA II: Trees, Graphs, Dynamic Programming, Advanced Structures
   4.1 Trees (traversals, diameter, LCA by binary lifting, tries)
   4.2 Graph Representations and Traversal (adjacency list vs CSR, BFS, 0-1 BFS, DFS, cycles)
   4.3 Shortest Paths (Dijkstra with stale entries, Bellman-Ford, Floyd-Warshall)
   4.4 Topological Sort and Disjoint-Set Union
   4.5 Dynamic Programming Patterns (linear, knapsack, LIS n log n, grid, interval, LCS/edit distance,
       bitmask, tree DP, digit DP, DAG DP, memory reduction)
   4.6 Segment Trees and Fenwick Trees (lazy propagation)
   4.7 String Algorithms (KMP prefix function — cited by 2.7/2.11, Z-function, rolling hash, Aho-Corasick overview)

## Part B — C++ depth   (ch05.md starts with:  #! Part B | C++ depth)
5  C++ Foundations Bridge
   5.1 The Compilation Model (preprocessor, translation units, linking, ODR, inline, headers)
   5.2 Values, References, Pointers and const
   5.3 Object Lifetime and Storage Duration
   5.4 Initialization (default/value/aggregate/list, most vexing parse)
   5.5 Classes: Constructors, Destructors, Rule of 0/3/5
   5.6 Integer Representation (two's complement, signed/unsigned pitfalls, promotions)
   5.7 Floating Point and Fixed-Point Prices
   5.8 Exceptions vs Error Codes (overview; costs in 8.6)
6  The C++ Object Model
   6.1 Object Layout, Alignment and Padding (cited by D4)
   6.2 Empty Base Optimization and [[no_unique_address]]
   6.3 Inheritance Layout and this-Pointer Adjustment
   6.4 vptr and vtable Mechanics
   6.5 The Cost of Virtual Dispatch and Devirtualization
   6.6 Alternatives: CRTP, std::variant, Function Pointers, Type Erasure
   6.7 Array of Structs vs Struct of Arrays
7  Memory Management
   7.1 Stack vs Heap
   7.2 How glibc malloc Works (arenas, bins, tcache, brk/mmap)
   7.3 new, delete, Placement New and Aligned New
   7.4 Page Faults, Pre-faulting and mlock (OS detail in 13.2)
   7.5 Custom Allocators: Arenas, Pools, std::pmr
   7.6 Smart Pointers and Their Real Costs
   7.7 No Allocation on the Hot Path: Enforcing It
8  Modern C++
   8.1 Value Categories · 8.2 Move Semantics (std::move is a cast — D7) · 8.3 Forwarding References
   and Perfect Forwarding · 8.4 Copy Elision and RVO · 8.5 RAII · 8.6 noexcept and the Cost of
   Exceptions · 8.7 Lambdas and std::function · 8.8 Vocabulary Types (optional, variant, span, string_view)
9  Compile-Time Programming
   9.1 Templates and Instantiation · 9.2 constexpr, consteval, constinit · 9.3 Type Traits and
   if constexpr · 9.4 SFINAE and Concepts · 9.5 CRTP · 9.6 Variadic Templates and Fold Expressions
   · 9.7 Costs: Code Bloat and Compile Time
10 STL Internals
   10.1 vector (growth, invalidation — D5) · 10.2 deque and list · 10.3 map and set (red-black trees)
   · 10.4 unordered_map · 10.5 Open-Addressing Hash Tables (full implementation) · 10.6 Custom Hashes,
   Comparators and Allocators · 10.7 Choosing a Container for the Hot Path
11 Undefined Behaviour and the Optimizer
   11.1 What UB Is · 11.2 Classic UB (signed overflow — D6, OOB, null, uninit, data races)
   · 11.3 Strict Aliasing and Type Punning (memcpy, bit_cast) · 11.4 Lifetime Bugs
   · 11.5 What the Optimizer Exploits (assembly) · 11.6 Detecting UB

## Part C — Systems   (ch12.md starts with:  #! Part C | Systems)
12 Computer Architecture
   12.1 Pipelines, Superscalar and Out-of-Order Execution · 12.2 Caches and Cache Lines (64 B — D8)
   · 12.3 Cache Coherence (MESI) · 12.4 Virtual-Address Translation and the TLB · 12.5 Branch Prediction
   · 12.6 SIMD Basics · 12.7 The Latency Hierarchy
13 Operating Systems and Linux
   13.1 Processes and Threads · 13.2 Virtual Memory and Page Faults · 13.3 Huge Pages
   · 13.4 Scheduling and Context Switches · 13.5 System Calls and the vDSO · 13.6 CPU Pinning and
   Isolation · 13.7 NUMA · 13.8 A Low-Latency Host Tuning Checklist
14 Concurrency
   14.1 Threads, Mutexes, Condition Variables · 14.2 Atomics and the C++ Memory Model (acquire/release
   — D10) · 14.3 x86-TSO · 14.4 False Sharing · 14.5 Lock-Free SPSC Ring Buffer · 14.6 Bounded MPMC
   Queue · 14.7 Seqlocks · 14.8 ABA and Memory Reclamation · 14.9 Spinning vs Blocking
15 Networking
   15.1 TCP vs UDP · 15.2 Multicast Market Data (D9) · 15.3 The Kernel Network Stack and Its Cost
   · 15.4 Kernel Bypass · 15.5 Timestamping and FPGAs
16 Compilers and Profiling
   16.1 Optimization Levels, LTO, PGO · 16.2 Inlining and Code Layout · 16.3 Reading x86-64 Assembly
   · 16.4 Benchmarking Correctly (rdtsc, percentiles) · 16.5 perf · 16.6 Sanitizers and gdb

## Part D — Trading-domain engineering   (ch17.md starts with:  #! Part D | Trading-domain engineering)
17 Market Microstructure for Engineers
   17.1 Exchanges, Participants, Order Types · 17.2 The Limit Order Book · 17.3 Matching Rules
   · 17.4 Market Data Feeds · 17.5 Order Lifecycle · 17.6 Tick-to-Trade · 17.7 Pre-Trade Risk Checks
18 Designing Low-Latency Components
   18.1 Order Book Data Structures · 18.2 Binary Feed Parser · 18.3 Matching Engine · 18.4 Rate Limiter
   · 18.5 Low-Latency Logging · 18.6 Representing Prices and Time · 18.7 System-Design Walkthroughs
19 Capstone Project (milestones M1–M9 as ## sections 19.1–19.9, then 19.10 Presenting the Project)

## Part E — Interviews and the internship   (ch20.md starts with:  #! Part E | Interviews and the internship)
20 Interview Strategy
   20.1 Thinking Aloud · 20.2 Handling What You Don't Know · 20.3 C++ and Systems Question Bank
   · 20.4 Mock Interview Sets
21 Converting the Internship into a PPO
   21.1 The First Two Weeks · 21.2 Scoping and Delivering · 21.3 Code Review, Testing, Measurement
   · 21.4 Mentors and the Final Presentation · 21.5 Why Interns Don't Convert

## Appendices
A Solutions (auto-built from solutions/chNN.md) · B Cheat Sheets (chapters/app_b.md)
· C Toolchain Setup (chapters/app_c.md) · D Reading List (chapters/app_d.md)
