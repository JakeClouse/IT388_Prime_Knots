#pragma once

// Functions 7 to 9 of the census.

#include <link/link.h>
#include <triangulation/dim3.h>

#include <functional>
#include <string>
#include <vector>

namespace prime_knots {

// Splits groups by key. Single knots go into finished.
std::vector<std::vector<int>> refine(const std::vector<std::vector<int>>& groups,
    const std::function<std::string(int)>& keyOf, std::vector<int>& finished);

// index is 2 to 7. Returns "" if there are too many generators.
std::string cover_homology(const regina::Link& knot, int index, int maxGenerators);

// mode is hyperbolic, satellite or prime
bool regina_certify(const regina::Triangulation<3>& complement, const std::string& mode);

} // namespace prime_knots
