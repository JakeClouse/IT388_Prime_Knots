// Stage one: candidate generation. See include/prime_knots/generate.hpp

#include "prime_knots/generate.hpp"

#include <cctype>
#include <cstdio>
#include <deque>
#include <exception>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

using namespace std;

namespace prime_knots {

namespace {

// Regina reads plantri's format only up to 26 nodes (letters a-z).
constexpr size_t max_plantri_nodes = 26;

// One code for each graph on the sphere, the same for a graph and its mirror
// image (plantri also treats mirror images as the same graph).
string code(const regina::ModelLinkGraph& graph) {
    return graph.canonicalPlantri(/*allowReflection=*/true, /*tight=*/false);
}

// Applies every single flype that Regina can find on `graph` and passes the
// code of each result to `visit`. Stops early if `visit` returns false.
template <typename Visit>
bool for_each_flype(const regina::ModelLinkGraph& graph, Visit&& visit) {
    for (size_t i = 0; i < graph.size(); ++i) {
        for (int a = 0; a < 4; ++a) {
            const regina::ModelLinkGraphArc from(graph.node(i), a);
            const auto [left, right] = graph.findFlype(from);
            if (!left || !right) continue; // no flype starts here
            string next;
            try {
                next = code(graph.flype(from, left, right));
            } catch (const exception&) {
                continue; // Regina refused the flype (a precondition failed)
            }
            if (!visit(next)) return false;
        }
    }
    return true;
}

// Closes a pipe from popen() even if an exception is thrown while reading it.
struct Pipe {
    FILE* file;
    explicit Pipe(FILE* f) : file(f) {}
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    ~Pipe() {
        if (file) pclose(file);
    }
    int close() {
        const int status = pclose(file);
        file = nullptr;
        return status;
    }
};

// Reads one line (without the newline). Returns false at end of input.
bool read_line(FILE* file, string& line) {
    line.clear();
    char buffer[4096];
    while (fgets(buffer, sizeof buffer, file)) {
        line += buffer;
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
            return true;
        }
    }
    return !line.empty();
}

// The checks and the alternating diagram for one plantri line 
// Returns true if the graph was passed to `action`.
bool handle_line(const string& line, size_t n, const GraphAction& action) {
    auto graph = parse_plantri_line(line);
    if (!graph) return false;
    if (graph->size() != n) return false;

    if (graph->countTraversals() != 1) return false;

    regina::Link alternating = graph->generateAnyLink(); // crossing k = node k
    alternating.makeAlternating();
    action(AlternatingCandidate{move(*graph), move(alternating)});
    return true;
}

} // namespace

string plantri_command(size_t n, size_t part, size_t parts, const string& plantri) {
    if (n < 1 || n > max_plantri_nodes)
        throw invalid_argument("crossing number must be between 1 and 26");
    if (parts < 1 || part >= parts)
        throw invalid_argument("need 0 <= part < parts");
    
    // -q: quadrangulations, -d: print the dual (our 4-valent graph), -a: text.
    // -c2m2: conditions 1-3 of paper section 3.1. A quadrangulation with n + 2
    // vertices has n faces, so its dual has n nodes.
    string command = plantri + " -adq -c2m2 " + to_string(n + 2);
    if (parts > 1) command += " " + to_string(part) + "/" + to_string(parts);
    return command;
}

optional<regina::ModelLinkGraph> parse_plantri_line(const string& line) {
    // Format: "<node count> <comma-separated adjacency>"
    size_t pos = line.find_first_not_of(" \t\r");
    if (pos == string::npos) return nullopt;
    const size_t digits = pos;
    while (pos < line.size() && isdigit(static_cast<unsigned char>(line[pos]))) ++pos;
    if (pos == digits) throw invalid_argument("plantri line must start with a node count");
    const size_t nodes = stoul(line.substr(digits, pos - digits));
    if (nodes > max_plantri_nodes)
        throw invalid_argument("Regina reads plantri output only up to 26 nodes");

    pos = line.find_first_not_of(" \t", pos);
    const size_t last = line.find_last_not_of(" \t\r");
    if (pos == string::npos || last < pos)
        throw invalid_argument("plantri line has no adjacency list");

    optional<regina::ModelLinkGraph> graph;
    try {
        graph.emplace(regina::ModelLinkGraph::fromPlantri(line.substr(pos, last - pos + 1)));
    } catch (const exception& error) {
        throw invalid_argument(string("bad plantri line: ") + error.what());
    }
    if (graph->size() != nodes)
        throw invalid_argument("plantri node count does not match the adjacency list");
    return graph;
}

// ---------------------------------------------------------------------------

size_t gen_alternating(istream& plantri_output, size_t n, const GraphAction& action) {
    size_t emitted = 0;
    for (string line; getline(plantri_output, line);)
        if (handle_line(line, n, action)) ++emitted;
    return emitted;
}

size_t gen_alternating(size_t n, size_t part, size_t parts, const GraphAction& action,
    const string& plantri) {
    const string command = plantri_command(n, part, parts, plantri);
    Pipe pipe(popen(command.c_str(), "r"));
    if (!pipe.file) throw runtime_error("cannot run: " + command);

    size_t emitted = 0;
    for (string line; read_line(pipe.file, line);)
        if (handle_line(line, n, action)) ++emitted;

    if (pipe.close() != 0) throw runtime_error("plantri failed: " + command);
    return emitted;
}

// ---------------------------------------------------------------------------
optional<string> flype_canonical(const regina::ModelLinkGraph& graph) {
    const string start = code(graph);
    unordered_set<string> seen{start};
    deque<string> pending{start};

    bool start_is_smallest = true;
    const auto visit = [&](const string& next) {
        if (next < start) {
            start_is_smallest = false; // another graph in the class will be kept
            return false;              // so stop searching
        }
        if (seen.insert(next).second) pending.push_back(next);
        return true;
    };

    while (!pending.empty()) {
        // A fresh copy for each step: findFlype caches data inside the graph.
        regina::ModelLinkGraph current = regina::ModelLinkGraph::fromPlantri(pending.front());
        pending.pop_front();
        if (!for_each_flype(current, visit)) break;
        current.reflect();
        if (!for_each_flype(current, visit)) break;
    }

    if (!start_is_smallest) return nullopt;
    return start;
}

// ---------------------------------------------------------------------------
size_t switch_crossings(const regina::ModelLinkGraph& graph, const DiagramAction& action) {
    unordered_set<string> seen;
    size_t emitted = 0;
    graph.generateMinimalLinks([&](regina::Link&& diagram) {
        if (diagram.countComponents() != 1) return; // safety: a link, not a knot
        if (diagram.isAlternating()) return;
        if (!seen.insert(diagram.sig()).second) return;
        ++emitted;
        action(move(diagram));
    });
    return emitted;
}

} // namespace prime_knots
