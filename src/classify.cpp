// The prime-knot stage for one diagram. Read classify() from top to bottom:
// each numbered step matches a step in docs/prime-pipeline.md.

#include "prime_knots/prime_knots.hpp"

#include <link/link.h>
#include <triangulation/dim3.h>

#include <exception>

namespace prime_knots {
namespace {

enum class Answer { yes, no, unknown };

// TODO(team): a crossing is "nugatory" (removable by untwisting) when its two
// opposite corners lie in the same face of the diagram.
//   for each crossing c: if opposite corners of c share a face: return no
//   return yes
Answer is_reduced(const regina::Link&) {
    return Answer::unknown;
}

// TODO(team): a circle that crosses the diagram at exactly two edges, with
// crossings on both sides, splits the knot as a connected sum.
//   for each pair of faces F, G sharing two edges e1, e2:
//       if both sides of the circle through F, e1, G, e2 have a crossing: return yes
//   return no
Answer is_visibly_composite(const regina::Link&) {
    return Answer::unknown;
}

// A strict angle structure proves the complement is hyperbolic, and hyperbolic
// knots are prime. The theorem needs every vertex of the triangulation to be
// ideal (its link is a torus).
bool proven_hyperbolic(const regina::Triangulation<3>& complement) {
    for (const auto* vertex : complement.vertices())
        if (!vertex->isIdeal()) return false;
    return complement.hasStrictAngleStructure();
    // TODO(team): if this fails, try other triangulations of the same
    // complement (Regina: Triangulation<3>::retriangulate) before giving up.
}

Report result(Report report, Status status, const std::string& reason) {
    report.status = status;
    report.reason = reason;
    return report;
}

} // namespace

Report classify(const std::string& line) {
    Report report;
    try {
        // 1. Read the diagram and build the knot.
        Candidate candidate;
        try {
            candidate = parse_candidate(line);
        } catch (const std::exception& error) {
            report.id = line.substr(0, line.find_first_of(" \t"));
            return result(report, Status::invalid_input, error.what());
        }
        report.id = candidate.id;
        report.crossings = candidate.pd.size();
        if (candidate.pd.empty())
            return result(report, Status::invalid_input, "no crossings: the unknot is not in the census");
        if (candidate.pd.size() > max_crossings)
            return result(report, Status::invalid_input, "more than 19 crossings");
        regina::Link knot;
        try {
            knot = regina::Link::fromPD(candidate.pd.begin(), candidate.pd.end());
        } catch (const std::exception& error) {
            return result(report, Status::invalid_input, error.what());
        }
        if (knot.countComponents() != 1)
            return result(report, Status::invalid_input, "this is a link, not a knot");
        if (!knot.isClassical())
            return result(report, Status::invalid_input, "virtual diagrams are not supported");

        // 2. If the knot has a diagram with fewer crossings, it is not a
        //    knot with n crossings and belongs to a smaller part of the census.
        regina::Link smaller = knot;
        smaller.simplifyToLocalMinimum();
        if (smaller.size() < knot.size())
            return result(report, Status::not_minimal, "Reidemeister moves removed crossings");
        const Answer reduced = is_reduced(knot);
        const Answer composite = is_visibly_composite(knot);
        if (reduced == Answer::no)
            return result(report, Status::not_minimal, "the diagram has a nugatory crossing");
        if (composite == Answer::yes)
            return result(report, Status::composite, "the diagram is visibly a connected sum");

        // 3. Shortcut: a reduced alternating diagram that is not visibly a
        //    connected sum is prime with exactly n crossings (Menasco;
        //    Kauffman-Murasugi-Thistlethwaite).
        const bool prime_alternating =
            knot.isAlternating() && reduced == Answer::yes && composite == Answer::no;

        // 4. Build the triangulation of the knot complement (the 3-D space
        //    around the knot). Regina simplifies it while building it; small
        //    triangulations make step 5 far more likely to succeed.
        const auto complement = knot.complement();
        report.signature = complement.isoSig();
        if (prime_alternating)
            return result(report, Status::prime_alternating, "reduced alternating diagram");

        // 5. Hyperbolic knots are prime.
        if (proven_hyperbolic(complement))
            return result(report, Status::prime_hyperbolic, "strict angle structure found");

        // 6. TODO(team), last: torus knots and satellite knots end up here.
        return result(report, Status::unresolved, "no proof found yet");
    } catch (const std::exception& error) {
        // Never let an exception escape: classify() runs inside OpenMP threads.
        return result(report, Status::unresolved, std::string("error: ") + error.what());
    }
}

} // namespace prime_knots
