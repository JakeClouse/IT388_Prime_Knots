#include "prime_knots/prove.hpp"

#include <algebra/abeliangroup.h>
#include <algebra/grouppresentation.h>
#include <surface/normalsurfaces.h>

#include <algorithm>
#include <map>
#include <stdexcept>

using namespace regina;
using std::vector, std::string, std::map;

namespace prime_knots {
namespace {

bool hasStrictAngles(const Triangulation<3>& tri) {
    for (const auto* vertex : tri.vertices())
        if (!vertex->isIdeal()) return false;
    return tri.hasStrictAngleStructure();
}

bool certifyHyperbolic(const Triangulation<3>& complement) {
    if (hasStrictAngles(complement)) return true;
    return complement.retriangulate(1, 1, nullptr,
        [](const string&, const Triangulation<3>& tri) {
            return hasStrictAngles(tri);
        });
}

bool isTorus(const NormalSurface& s) {
    if (!s.isCompact()) return false; // check this first
    if (!s.isConnected() || !s.isOrientable() || !s.isTwoSided()) return false;
    if (s.isVertexLinking()) return false;
    return s.eulerChar() == 0;
}

bool isAnnulus(const NormalSurface& s) {
    if (!s.isCompact()) return false;
    if (!s.isConnected() || !s.isOrientable() || !s.isTwoSided()) return false;
    if (!s.hasRealBoundary()) return false;
    return s.eulerChar() == 0;
}

// Looks for a torus that makes the knot a satellite.
bool certifySatellite(const Triangulation<3>& complement) {
    NormalSurfaces list(complement, NormalCoords::Quad, NormalList::Vertex);
    for (const NormalSurface& s : list) {
        if (!isTorus(s)) continue;
        vector<Triangulation<3>> pieces = s.cutAlong().triangulateComponents();
        if (pieces.size() != 2) continue;

        // cusp is the piece touching the knot
        Triangulation<3> cusp = pieces[0].isIdeal() ? pieces[0] : pieces[1];
        Triangulation<3> outside = pieces[0].isIdeal() ? pieces[1] : pieces[0];

        if (outside.isSolidTorus()) continue;
        if (cusp.isTxI()) continue;
        cusp.idealToFinite();
        if (cusp.hasCompressingDisc()) continue;
        return true;
    }
    return false;
}

// Looks for an annulus that splits the knot in two.
bool certifyPrime(const Triangulation<3>& complement) {
    Triangulation<3> finite(complement);
    finite.idealToFinite();
    finite.simplify();
    NormalSurfaces list(finite, NormalCoords::Standard, NormalList::Fundamental);
    for (const NormalSurface& s : list) {
        if (!isAnnulus(s)) continue;
        vector<Triangulation<3>> pieces = s.cutAlong().triangulateComponents();
        if (pieces.size() != 2) continue;
        if (!pieces[0].isSolidTorus() && !pieces[1].isSolidTorus()) return false;
    }
    return true;
}

} // namespace

vector<vector<int>> refine(const vector<vector<int>>& groups,
    const std::function<string(int)>& keyOf, vector<int>& finished) {
    vector<vector<int>> newGroups;
    for (const vector<int>& group : groups) {
        map<string, vector<int>> buckets;
        for (int knot : group)
            buckets[keyOf(knot)].push_back(knot);

        for (const auto& [key, members] : buckets) {
            if (members.size() == 1)
                finished.push_back(members[0]);
            else
                newGroups.push_back(members);
        }
    }
    return newGroups;
}

string cover_homology(const Link& knot, int index, int maxGenerators) {
    GroupPresentation group = knot.group();
    if ((int)group.countGenerators() > maxGenerators) return "";

    vector<string> homologies;
    auto action = [&](GroupPresentation&& sub) {
        homologies.push_back(sub.abelianisation().str());
    };

    // enumerateCovers needs a constant
    switch (index) {
        case 2: group.enumerateCovers<2>(action); break;
        case 3: group.enumerateCovers<3>(action); break;
        case 4: group.enumerateCovers<4>(action); break;
        case 5: group.enumerateCovers<5>(action); break;
        case 6: group.enumerateCovers<6>(action); break;
        case 7: group.enumerateCovers<7>(action); break;
        default: throw std::invalid_argument("index must be between 2 and 7");
    }

    std::sort(homologies.begin(), homologies.end());
    string key = "k=" + std::to_string(index) + ":";
    for (const string& h : homologies)
        key += h + ";";
    return key;
}

bool regina_certify(const Triangulation<3>& complement, const string& mode) {
    if (mode == "hyperbolic") return certifyHyperbolic(complement);
    if (mode == "satellite") return certifySatellite(complement);
    if (mode == "prime") return certifyPrime(complement);
    throw std::invalid_argument("unknown mode: " + mode);
}

} // namespace prime_knots
