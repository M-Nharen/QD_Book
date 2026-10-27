// book: flags=-pthread
// Showdown, naive side (Section 14.10): mutex + condition-variable queue. tsan-clean.
#include <cstdio>
#include "blocking_queue.hpp"
#include "showdown_harness.hpp"

int main() {
    BlockingQueue<Quote> q(1024);
    std::printf("BlockingQueue<Quote>(1024): mutex + two condition variables\n");
    run_showdown([&](const Quote& m) { q.push(m); },
                 [&] { return *q.pop(); });
}
