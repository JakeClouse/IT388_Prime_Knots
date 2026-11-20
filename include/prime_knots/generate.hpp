#pragma once

// Stage one of the census: build the candidate knot diagrams.
// Follows section 3 of Burton, "The Next 350 Million Knots" (SoCG 2020).
// Pseudocode and reasoning: docs/generation-pseudocode.md.
//
//   1. gen_alternating   plantri -> every allowed 4-valent graph with n nodes,
//                        plus its alternating diagram          (paper 3.1)
//   2. flype_canonical   keep one graph from each flype class  (paper 3.2)
//   3. switch_crossings  one kept graph -> its non-alternating
//                        candidate diagrams                    (paper 3.3)
//
// Thread safety: every function works on its own copies, so different threads
// may call them at the same time. Do not share one ModelLinkGraph object
// between threads: Regina caches some data inside it without locking.

#include <link/link.h>
#include <link/modellinkgraph.h>

#include <cstddef>
#include <functional>
#include <istream>
#include <optional>
#include <string>

namespace prime_knots {

// One graph from plantri that passed every check, with its alternating diagram.
// Crossing k of `alternating` sits on node k of `graph`.
struct AlternatingCandidate {
    regina::ModelLinkGraph graph;
    regina::Link alternating;
};

using GraphAction = std::function<void(AlternatingCandidate&&)>;
using DiagramAction = std::function<void(regina::Link&&)>;

// The plantri command line for n-crossing graphs, part `part` of `parts`.
// Example: plantri_command(17, 0, 8) == "plantri -adq -c2m2 19 0/8".
// The parts split the graphs into independent jobs (plantri's res/mod).
std::string plantri_command(std::size_t n, std::size_t part = 0, std::size_t parts = 1,
    const std::string& plantri = "plantri");

// Reads one line of `plantri -adq` output, e.g. "6 bcdd,aeec,abfd,acfa,bffb,ceed".
// Returns nothing for a blank line; throws std::invalid_argument if malformed.
std::optional<regina::ModelLinkGraph> parse_plantri_line(const std::string& line);

// 1. Reads plantri output from `plantri_output` and calls `action` once for
//    every graph with n nodes that models a knot (one component), not a link.
//    Returns how many graphs were passed to `action`.
std::size_t gen_alternating(std::istream& plantri_output, std::size_t n, const GraphAction& action);

// 1. Same, but runs plantri itself (part `part` of `parts`).
//    Throws std::runtime_error if plantri cannot be started or fails.
std::size_t gen_alternating(std::size_t n, std::size_t part, std::size_t parts,
    const GraphAction& action, const std::string& plantri = "plantri");

// 2. Returns the graph's canonical plantri code if it is the smallest graph in
//    its flype class (so it should be kept), or nothing if another graph in
//    the class is smaller (so it should be dropped). Exactly one graph per
//    class is kept, and no graphs need to be compared across processes.
//    Precondition: `graph` came from gen_alternating (planar, prime, a knot).
std::optional<std::string> flype_canonical(const regina::ModelLinkGraph& graph);

// 3. Calls `action` once for every non-alternating, one-component diagram on
//    `graph` that Regina cannot rule out as non-minimal. A diagram and its
//    mirror image count once. Returns how many diagrams were passed on.
//    Precondition: as for flype_canonical; use only on graphs that it keeps.
std::size_t switch_crossings(const regina::ModelLinkGraph& graph, const DiagramAction& action);

} // namespace prime_knots
