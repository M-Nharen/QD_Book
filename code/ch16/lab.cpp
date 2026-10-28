// book: timeout=600
// The flags lab (Sections 16.1 and 16.6). Compiles the small programs in code/ch16/lab
// with different g++ flags into a temporary directory, runs each build three times and
// reports the median of what the program prints (nanoseconds per operation).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static const fs::path kLab = fs::path(__FILE__).parent_path() / "lab";
static fs::path tmp;

static void sh(const std::string& cmd) {             // compile steps must succeed
    if (std::system(cmd.c_str()) != 0) {
        std::fprintf(stderr, "failed: %s\n", cmd.c_str());
        std::exit(1);
    }
}

static std::string build(const std::string& flags, const std::string& srcs,
                         const std::string& exe) {
    std::string cmd = "g++ -std=c++20 -Wall -Wextra -Werror " + flags;
    for (std::size_t a = 0, b; a < srcs.size(); a = b + 1) {
        b = std::min(srcs.find(' ', a), srcs.size());
        cmd += " " + (kLab / srcs.substr(a, b - a)).string();
    }
    sh(cmd + " -o " + (tmp / exe).string());
    return (tmp / exe).string();
}

// Median over three runs of each number the program prints on its one line of output.
static std::vector<double> run(const std::string& exe, const std::string& args = "") {
    std::vector<std::vector<double>> runs;
    for (int r = 0; r < 3; ++r) {
        FILE* p = popen((exe + " " + args).c_str(), "r");
        std::vector<double> v;
        double x;
        while (p && std::fscanf(p, "%lf", &x) == 1) v.push_back(x);
        if (!p || pclose(p) != 0 || v.empty()) {
            std::fprintf(stderr, "run failed: %s\n", exe.c_str());
            std::exit(1);
        }
        runs.push_back(v);
    }
    std::vector<double> med(runs[0].size());
    for (std::size_t i = 0; i < med.size(); ++i) {
        double c[3] = {runs[0][i], runs[1][i], runs[2][i]};
        std::sort(c, c + 3);
        med[i] = c[1];
    }
    return med;
}

int main() {
    char dir[] = "/tmp/qd16labXXXXXX";
    if (!mkdtemp(dir)) return 1;
    tmp = dir;

    std::printf("1. Reductions over 4,096 elements, ns per element (reduce.cc)\n");
    std::printf("   %-34s %8s %8s\n", "flags", "int", "float");
    for (const char* f : {"-O0", "-O1", "-O2", "-O3", "-O3 -march=native",
                          "-O3 -march=native -ffast-math"}) {
        const auto v = run(build(f, "reduce.cc", "reduce"));
        std::printf("   %-34s %8.3f %8.3f\n", f, v[0], v[1]);
    }

    std::printf("2. Call into another translation unit, ns per call (lto_*.cc)\n");
    for (const char* f : {"-O2", "-O2 -flto"})
        std::printf("   %-34s %8.3f\n", f,
                    run(build(f, "lto_main.cc lto_lib.cc", "lto"))[0]);

    std::printf("3. Virtual dispatch, 96%% to one type, ns per call (dispatch.cc)\n");
    const double plain = run(build("-O2", "dispatch.cc", "dispatch"))[0];
    const std::string gen = "-O2 -fprofile-dir=" + tmp.string();
    run(build(gen + " -fprofile-generate", "dispatch.cc", "pgo"), "500");   // training
    const double pgo = run(build(gen + " -fprofile-use -Wmissing-profile", "dispatch.cc",
                                 "pgo"))[0];
    std::printf("   %-34s %8.3f\n   %-34s %8.3f\n", "-O2", plain,
                "-O2 -fprofile-use (after training)", pgo);

    std::printf("4. Sanitizer cost, ns per operation (workload.cc)\n");
    const char* base = "-O1 -g -fno-omit-frame-pointer";
    const double b = run(build(base, "workload.cc", "w0"))[0];
    std::printf("   %-34s %8.1f   1.0x\n", "-O1 -g (baseline)", b);
    int k = 1;
    for (const char* san : {"-fsanitize=address", "-fsanitize=undefined",
                            "-fsanitize=address,undefined", "-fsanitize=thread"}) {
        const double t = run(build(std::string(base) + " " + san, "workload.cc",
                                   "w" + std::to_string(k++)))[0];
        std::printf("   %-34s %8.1f %5.1fx\n", san, t, t / b);
    }
    fs::remove_all(tmp);
}
