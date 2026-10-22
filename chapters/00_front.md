# How to Use This Book

This book is meant to be worked through, not read once. Each technical chapter follows the same template, so you always know where to look:

1. **Intuition from zero.** The plain-language idea, with an analogy where one helps.
2. **Low-level mechanics.** What the compiler, operating system and hardware actually do.
3. **Code showdown.** A straightforward version against an optimized version, both complete and compiled.
4. **The nanosecond penalty.** What the difference costs in cycles or nanoseconds, given as approximate ranges with the reason for each number.
5. **Interview questions** with model answers.
6. **Practice problems,** graded **[E]** easy, **[M]** medium, **[H]** hard. Full solutions are in Appendix A, never next to the problem, so you cannot glance at them by accident.
7. **Key takeaways.** A one-page summary for revision the night before an interview.

For the probability and algorithms chapters the template is adapted. "Low-level mechanics" becomes "how to set the problem up". The code showdown becomes a fast, trustworthy way to verify answers or a cache-friendly implementation.

## *Conventions

- **Code.** Every listing is a complete program. The build script compiles every listing with `g++ -O2 -std=c++20 -Wall -Wextra -Werror` (GCC 13.3) and runs it; a listing that fails to compile, warns, or exits with an error stops the book from building. Where a listing's output is shown, it is the real output captured at build time. Line numbers in listings are referenced in the text as "line 12".
- **Numbers.** Latency and throughput figures depend on the CPU model, clock speed, memory, operating system configuration and load. Where this book gives such a number without measuring it, it is an approximate range for recent x86-64 server CPUs, labelled as such, with the reason it falls in that range. Where the book shows a measured number, it was measured in the build environment: a virtualized Linux container with 2 vCPUs. Treat those as illustrations of *relative* cost, not as what you would see on an isolated, tuned bare-metal trading server.
- **Firm-specific facts.** This book does not claim to know any particular firm's interview format, question bank or compensation. Chapter 1 describes patterns that are common across the industry and tells you how to verify specifics for the firms you apply to.
- **Cross-references.** Topics are explained once. Later chapters point back ("see Section 3.4") instead of repeating material.

## *Suggested path

If your online tests are less than a month away, do Chapters 1–4 first and in order: they are what gets you through the screen. Then do Chapters 5–11 (C++ depth), which most QD technical rounds probe heavily, and Chapters 12–16 (systems). Part D is what separates a strong QD candidate from a strong general software candidate; the capstone in Chapter 19 gives you a project you can discuss in depth. Chapter 1 contains a 12-week plan and a compressed 4-week plan.

## *Tools you need

A Linux environment (native, or WSL2 on Windows), GCC 12 or later or Clang 15 or later, and a browser for Compiler Explorer (godbolt.org). Appendix C covers setup in detail. For Part A, a compiler and a text editor are enough.
