// Input parsing and JSON output. Runs without Regina.

#include "prime_knots/prime_knots.hpp"

#include <iostream>
#include <stdexcept>

using namespace prime_knots;

namespace {
int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures;
    }
}

bool rejected(const std::string& line) {
    try {
        parse_candidate(line);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
} // namespace

int main() {
    const auto trefoil = parse_candidate("trefoil 1 5 2 4 3 1 4 6 5 3 6 2");
    check(trefoil.id == "trefoil", "reads the name");
    check(trefoil.pd.size() == 3, "reads 3 crossings");
    check(trefoil.pd[1] == Crossing{3, 1, 4, 6}, "keeps label order");
    check(parse_candidate("unknot").pd.empty(), "a name alone means no crossings");

    check(rejected(""), "empty line");
    check(rejected("x 1 2 3"), "incomplete crossing");
    check(rejected("x 1 2 3 4junk"), "junk after a number");
    check(rejected("x 1 2 3 99999999999999999999"), "number too large");
    check(rejected("x 1 1 1 2"), "label used more than twice");
    check(rejected("x 0 1 1 2"), "label 0");
    check(rejected("x 1 2 3 4"), "label above 2n");

    Report report;
    report.id = "a\"b\\c\n";
    report.crossings = 3;
    report.status = Status::prime_hyperbolic;
    report.reason = "why";
    report.signature = "sig";
    check(to_json(report) ==
        R"({"id":"a\"b\\c\u000a","crossings":3,"status":"PRIME_HYPERBOLIC","reason":"why","signature":"sig"})",
        "JSON output and escaping");

    if (failures == 0) std::cout << "Parse checks passed\n";
    return failures == 0 ? 0 : 1;
}
