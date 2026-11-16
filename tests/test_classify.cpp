// Classification of known knots. Needs Regina.

#include "prime_knots/prime_knots.hpp"

#include <iostream>
#include <string>

using namespace prime_knots;

namespace {
int failures = 0;

void expect(const std::string& line, Status want) {
    const auto report = classify(line);
    if (report.status != want) {
        std::cerr << "FAILED: " << report.id << " gave " << to_string(report.status)
                  << " (" << report.reason << "), expected " << to_string(want) << '\n';
        ++failures;
    }
}

// n crossings that each just twist the strand: an unknot drawn badly.
std::string curls(int n) {
    std::string line = "curls";
    for (int i = 0; i < n; ++i) {
        const int a = 2 * i + 1;
        line += " " + std::to_string(a) + " " + std::to_string(a + 1) + " " +
            std::to_string(a + 1) + " " + std::to_string(i + 1 == n ? 1 : a + 2);
    }
    return line;
}
} // namespace

int main() {
    // Hyperbolic, so proven prime.
    expect("figure-eight 4 2 5 1 8 6 1 5 6 3 7 4 2 7 3 8", Status::prime_hyperbolic);
    // Torus knot: prime but not hyperbolic, so stays unresolved until step 6.
    expect("trefoil 1 5 2 4 3 1 4 6 5 3 6 2", Status::unresolved);
    // Composite: must never be reported as prime.
    expect("trefoil-sum-trefoil 1 5 2 4 3 1 4 6 5 3 12 2 7 11 8 10 9 7 10 12 11 9 6 8",
        Status::unresolved);
    // Simplifies to fewer crossings.
    expect(curls(5), Status::not_minimal);
    // Inputs outside the census.
    expect("unknot", Status::invalid_input);
    expect(curls(20), Status::invalid_input);
    expect("hopf-link 1 4 2 3 3 2 4 1", Status::invalid_input);
    expect("virtual 1 2 3 4 3 4 2 1", Status::invalid_input);
    expect("bad 1 2 3", Status::invalid_input);

    if (failures == 0) std::cout << "Classification checks passed\n";
    return failures == 0 ? 0 : 1;
}
