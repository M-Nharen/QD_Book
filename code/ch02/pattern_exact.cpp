// Exact waiting times and "which pattern first" probabilities for i.i.d. symbol streams.
// States = longest suffix of the history that is a prefix of some pattern (KMP idea, Ch 4).
// Solve the absorbing Markov chain (I - Q) x = b by Gaussian elimination (Section 2.7).
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct Answer {
    double expected_time;            // E[steps until any pattern appears]
    std::vector<double> first_prob;  // P(pattern k is the first to appear)
};

// Solve A x = B for several right-hand sides at once (columns of B). A is n x n.
std::vector<std::vector<double>> gauss(std::vector<std::vector<double>> A,
                                       std::vector<std::vector<double>> B) {
    const std::size_t n = A.size(), m = B[0].size();
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;                                   // partial pivoting
        for (std::size_t r = col + 1; r < n; ++r)
            if (std::fabs(A[r][col]) > std::fabs(A[piv][col])) piv = r;
        std::swap(A[col], A[piv]);
        std::swap(B[col], B[piv]);
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col || A[r][col] == 0.0) continue;
            double f = A[r][col] / A[col][col];
            for (std::size_t c = col; c < n; ++c) A[r][c] -= f * A[col][c];
            for (std::size_t c = 0; c < m; ++c) B[r][c] -= f * B[col][c];
        }
    }
    for (std::size_t r = 0; r < n; ++r)
        for (std::size_t c = 0; c < m; ++c) B[r][c] /= A[r][r];
    return B;
}

Answer solve(const std::vector<std::string>& pats, const std::string& alphabet,
             const std::vector<double>& prob) {
    std::map<std::string, int> id;                               // transient states
    for (const auto& p : pats)
        for (std::size_t len = 0; len < p.size(); ++len) id.emplace(p.substr(0, len), 0);
    int n = 0;
    for (auto& [s, i] : id) i = n++;
    auto is_prefix = [&](const std::string& t) { return id.count(t) > 0; };

    const std::size_t K = pats.size();
    std::vector<std::vector<double>> A(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> B(n, std::vector<double>(K + 1, 0.0));
    for (auto& [s, i] : id) {
        A[i][i] += 1.0;
        B[i][K] = 1.0;                                           // the "+1 step" term
        for (std::size_t a = 0; a < alphabet.size(); ++a) {
            std::string t = s + alphabet[a];
            int hit = -1;
            for (std::size_t k = 0; k < K; ++k)                  // did a pattern just end?
                if (t.size() >= pats[k].size() &&
                    t.compare(t.size() - pats[k].size(), pats[k].size(), pats[k]) == 0)
                    hit = int(k);
            if (hit >= 0) { B[i][hit] += prob[a]; continue; }    // absorbed
            while (!is_prefix(t)) t.erase(0, 1);                 // fall back (KMP failure)
            A[i][id[t]] -= prob[a];
        }
    }
    auto X = gauss(A, B);
    int start = id[""];
    Answer ans{X[start][K], {}};
    for (std::size_t k = 0; k < K; ++k) ans.first_prob.push_back(X[start][k]);
    return ans;
}

int main() {
    const std::vector<double> fair{0.5, 0.5};
    std::printf("Expected waiting time, fair coin:\n");
    for (const char* p : {"HH", "HT", "HHH", "HTH", "HHT", "HTHT", "HHHHH"})
        std::printf("  %-6s %8.4f\n", p, solve({p}, "HT", fair).expected_time);

    std::vector<double> die(6, 1.0 / 6);
    std::printf("Fair die, two consecutive sixes: %.4f\n",
                solve({"66"}, "123456", die).expected_time);

    std::printf("Biased coin P(H)=0.6, HH: %.4f   (formula (1+p)/p^2 = %.4f)\n",
                solve({"HH"}, "HT", {0.6, 0.4}).expected_time, (1 + 0.6) / (0.6 * 0.6));

    std::printf("Races (probability the first pattern wins), fair coin:\n");
    for (auto [a, b] : std::vector<std::pair<const char*, const char*>>{
             {"THH", "HHH"}, {"HHT", "HTH"}, {"HTT", "TTH"}, {"HT", "TT"}}) {
        Answer r = solve({a, b}, "HT", fair);
        std::printf("  %s vs %s : %.4f   (E[time] %.4f)\n", a, b, r.first_prob[0],
                    r.expected_time);
    }
}
