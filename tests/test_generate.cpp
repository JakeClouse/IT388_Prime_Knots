// Candidate generation checks. Needs Regina.
// Usage: test-generate [PLANTRI]
// Without PLANTRI only the parsing checks run. With it, the number of graphs
// kept by flype_canonical must equal the known number of prime alternating
// knots for each crossing number (Tait flyping theorem).

#include "prime_knots/generate.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace prime_knots;

namespace {
int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures;
    }
}

void parsing_checks() {
    // Example from the Regina docs for ModelLinkGraph::fromPlantri.
    const auto graph = parse_plantri_line("6 bcdd,aeec,abfd,acfa,bffb,ceed");
    check(graph && graph->size() == 6, "parse a 6-node plantri line");
    check(!parse_plantri_line("   "), "blank line gives no graph");

    bool threw = false;
    try {
        parse_plantri_line("bcdd,aeec");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "line without a node count is rejected");

    check(plantri_command(17) == "plantri -adq -c2m2 19", "plantri command");
    check(plantri_command(17, 3, 8) == "plantri -adq -c2m2 19 3/8", "plantri command with parts");
}

void census_checks(const std::string& plantri) {
    // Prime alternating knots with n crossings, counted up to mirror image
    // (OEIS A002864).
    const std::map<std::size_t, std::size_t> alternating = {
        {3, 1}, {4, 1}, {5, 2}, {6, 3}, {7, 7}, {8, 18}, {9, 41}, {10, 123}};

    for (const auto& [n, want] : alternating) {
        std::size_t graphs = 0, kept = 0, diagrams = 0;
        gen_alternating(n, 0, 1, [&](AlternatingCandidate&& candidate) {
            ++graphs;
            check(candidate.alternating.size() == n && candidate.alternating.isAlternating() &&
                    candidate.alternating.countComponents() == 1,
                "alternating diagram with " + std::to_string(n) + " crossings");
            if (!flype_canonical(candidate.graph)) return;
            ++kept;
            diagrams += switch_crossings(candidate.graph, [&](regina::Link&& diagram) {
                check(diagram.size() == n && !diagram.isAlternating() &&
                        diagram.countComponents() == 1,
                    "non-alternating knot diagram with " + std::to_string(n) + " crossings");
            });
        }, plantri);

        std::cout << "n=" << n << " graphs=" << graphs << " kept=" << kept
                  << " non-alternating candidates=" << diagrams << '\n';
        check(kept == want, "n=" + std::to_string(n) + ": kept " + std::to_string(kept) +
            " graphs, expected " + std::to_string(want));
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        parsing_checks();
        if (argc > 1) census_checks(argv[1]);
        else std::cout << "plantri not given: skipped census checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << '\n';
        ++failures;
    }
    if (failures == 0) std::cout << "Generation checks passed\n";
    return failures == 0 ? 0 : 1;
}
