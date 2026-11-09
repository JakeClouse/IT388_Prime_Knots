#pragma once

// Prime-knot stage: take one knot diagram, build the triangulation of its
// complement with Regina, and decide whether the knot is a prime knot that
// belongs in the census. Pseudocode: docs/prime-pipeline.md.

#include <array>
#include <string>
#include <vector>

namespace prime_knots {

// The census covers knots with at most 19 crossings.
inline constexpr std::size_t max_crossings = 19;

// One crossing in PD (planar diagram) notation: the labels of the four arcs
// that meet there, in Regina's (= KnotInfo's) order.
using Crossing = std::array<int, 4>;

// One input line: a name followed by 4 arc labels per crossing.
// Example: "trefoil 1 5 2 4 3 1 4 6 5 3 6 2"
struct Candidate {
    std::string id;
    std::vector<Crossing> pd;
};

enum class Status {
    prime_alternating, // proven prime, crossing number exactly n
    prime_hyperbolic,  // proven prime, crossing number at most n
    not_minimal,       // a diagram with fewer crossings exists
    composite,         // diagram is visibly a connected sum
    unresolved,        // no proof either way yet (never means "not prime")
    invalid_input      // not a single knot diagram with 1-19 crossings
};

struct Report {
    std::string id;
    std::size_t crossings = 0;
    Status status = Status::unresolved;
    std::string reason;    // one sentence explaining the status
    std::string signature; // Regina isoSig of the simplified complement, or empty
};

// candidate.cpp (no Regina needed)
Candidate parse_candidate(const std::string& line); // throws std::invalid_argument
std::string to_string(Status);
std::string to_json(const Report&);

// classify.cpp (uses Regina). Never throws; safe to call from many threads.
Report classify(const std::string& line);

} // namespace prime_knots
