// The prime-knot stage for one diagram. Read classify() from top to bottom:
// each numbered step matches a step in docs/prime-pipeline.md.

#include "prime_knots/prime_knots.hpp"

#include <link/link.h>
#include <triangulation/dim3.h>
#include <snappea/snappeatriangulation.h>   // CHECK: include path
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <exception>

using namespace regina;
using std::vector, std::string, std::unordered_map;

namespace prime_knots {
namespace {
int DEFAULT_PREFILTER_MOVES = 10;
struct r3Move {
    regina::Crossing* crossing;
    int side;
};

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

//Claude used with reasearch paper for pseudocode
bool checkNoR1R2(const regina::Link& knot) {
    for (int i = 0; i < knot.crossings().size(); i++){
        if (knot.hasR1(knot.crossings()[i])){
            return false;
        }
        if (knot.hasR2(knot.crossings()[i])){
            return false;
        }
    }
    return true;
}
//Claude used with reasearch paper for pseudocode
bool randomR3Prefilter(const regina::Link& knot) {
    regina::Link knotCopy(knot);
    std::vector<r3Move> r3Moves;

    for (int i = 0; i < DEFAULT_PREFILTER_MOVES; ++i) {
        if (!checkNoR1R2(knotCopy)){
            return false;
        }
        r3Moves.clear();
        for (int j = 0; j < knotCopy.size(); j++){
            if (knotCopy.hasR3(knotCopy.crossings()[j],1)){
                r3Moves.push_back({knotCopy.crossings()[j],1});
            }
            if (knotCopy.hasR3(knotCopy.crossings()[j],0)){
                r3Moves.push_back({knotCopy.crossings()[j],0});
            }
        }
        if (!r3Moves.empty()) {
            int randomIndex = rand() % r3Moves.size();
            knotCopy.r3(r3Moves[randomIndex].crossing, r3Moves[randomIndex].side);
        }else{
            return true;
        }
    }
    return checkNoR1R2(knotCopy);
}
//Claude used with reasearch paper for pseudocode
bool reducingPassCheck(const Link& knot) {
    return !knot.hasReducingPass();
} 
//Claude used with reasearch paper for pseudocode
bool simplifyingFilter(const Link& knot, int extraCrossings, int numThreads) {
    const  size_t n = knot.size();
    Link knotCopy(knot);
    knotCopy.simplify();
    if (knotCopy.size() < n) {
        return false;
    }
    Link knotCopy2(knot);
    knotCopy2.simplifyExhaustive(extraCrossings, numThreads,nullptr);
    return knotCopy2.size() >= n;
} 
//Claude used with reasearch paper for pseudocode
Link r3Canonical(const Link& knot, int numThreads) {
    Link knotCopy(knot);
    string best = knotCopy.sig();
//Claude used to help with lambda formatting, as i am unfamilliar with them.
    knotCopy.rewrite(0, numThreads, 
        nullptr,
        [&best](const std::string& sig, const regina::Link&) {
        if (sig < best) {
            best = sig;
        }
            return false;
        });
    return Link(best);
}
//Claude used with reasearch paper for pseudocode
vector<int> connectKnots(const vector<Link>& knots, int extraCrossings, int numThreads) {
    unordered_map<string, size_t> owner;
    vector<int> label(knots.size());
    for(int i = 0; i < knots.size(); ++i) {
        label[i] = i;
        knots[i].rewrite(extraCrossings, numThreads, nullptr,
            //Lambda function created with llm assistance
            [&](const std::string& sig, const regina::Link&) {
                auto r = owner.emplace(sig, i);      // claims sig if unclaimed
                if (r.second) return false;          // new diagram, keep searching
                label[i] = label[r.first->second];   // met an earlier search: join its class
                return true;                         // stop, its class already explored this region
            });
    }
    return label;
}
//Claude used with reasearch paper for pseudocode
string snappyTri(const Link& knot, int tries){
    SnapPeaTriangulation tri(knot);
    for (int i = 0; i < tries; i++){
        try {
            if (i > 0) tri.randomise();
            SnapPeaTriangulation::Solution s = tri.solutionType();
            if (s != SnapPeaTriangulation::Solution::Geometric &&
                s != SnapPeaTriangulation::Solution::Nongeometric) continue;
            return tri.canonise().isoSig();

        }catch (const std::exception&) {
            tri = SnapPeaTriangulation(knot);              
        }
    }
    return "";
}

string polynomial(const Link& knot, bool homefly){
    Link mirror(knot);
    mirror.reflect();
    string a, b;
    if (homefly){
        a = knot.homfly().str();
        b = mirror.homfly().str();
    }else{
        a = knot.jones().str();
        b = mirror.jones().str();
    }
    return std::min(a,b);
}


}
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
