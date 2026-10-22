// const& promises that *this function* will not write through the reference.
// It does not promise that nobody else will (Section 5.2).
#include <cstdio>
#include <vector>

void scale_ref(std::vector<double>& v, const double& f) {   // f may alias an element
    for (double& x : v) x /= f;                               // must re-read f each time
}

void scale_val(std::vector<double>& v, double f) {          // f is a private copy
    for (double& x : v) x /= f;
}

void print(const char* name, const std::vector<double>& v) {
    std::printf("%-10s", name);
    for (double x : v) std::printf(" %5.2f", x);
    std::printf("\n");
}

int main() {
    std::vector<double> a{2, 4, 6, 8}, b = a;
    scale_ref(a, a[0]);         // "normalize by the first element"
    scale_val(b, b[0]);
    print("by ref:", a);        // a[0] became 1.0 after the first division
    print("by value:", b);

    int x = 10;
    const int& cr = x;          // a read-only *view* of a mutable object
    x = 20;
    std::printf("cr = %d (the object changed under a const reference)\n", cr);
}
