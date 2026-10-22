// book: flags=-pthread
// The four storage durations, made visible (Section 5.3). Checked under tsan as well.
#include <cstdint>
#include <cstdio>
#include <thread>

struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) { std::printf("  construct %s\n", name); }
    ~Tracer() { std::printf("  destroy   %s\n", name); }
};

Tracer g_static("static, namespace scope");      // constructed before main runs

Tracer& registry() {
    static Tracer r("static, function-local");   // constructed on the first call only
    return r;
}

thread_local long tl_count = 0;                  // one independent copy per thread

std::uintptr_t bump(const char* who) {           // returns the address it used
    for (int i = 0; i < 3; ++i) ++tl_count;
    std::printf("  %s: tl_count = %ld\n", who, tl_count);
    return reinterpret_cast<std::uintptr_t>(&tl_count);
}

int main() {
    std::printf("main starts\n");
    {
        Tracer a("automatic, block scope");
        registry();
        registry();                              // no second construction
        Tracer* d = new Tracer("dynamic (new)");
        delete d;                                // lifetime ends exactly here
    }                                            // a is destroyed here
    std::uintptr_t worker_copy = 0;
    std::thread t([&] { worker_copy = bump("worker thread"); });
    t.join();
    std::uintptr_t main_copy = bump("main thread  ");
    std::printf("  distinct objects: %s\n", worker_copy != main_copy ? "yes" : "no");
    std::printf("main ends\n");
}   // statics are destroyed after main returns, in reverse order of construction
