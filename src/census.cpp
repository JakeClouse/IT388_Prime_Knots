// census MAX_CROSSINGS [--plantri PATH] [--csv FILE]
// Generates and proves every prime knot with 3 to MAX_CROSSINGS crossings.

#include "prime_knots/generate.hpp"
#include "prime_knots/prove.hpp"

#include <link/link.h>
#include <snappea/snappeatriangulation.h>
#include <triangulation/dim3.h>

#include <mpi.h>
#include <omp.h>

#include <climits>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace prime_knots;
using regina::Link;
using regina::SnapPeaTriangulation;
using regina::Triangulation;

const int TRIES = 20;

// Prime knots with n crossings, up to mirror image (OEIS A002863).
const long known[] = {0, 0, 0, 1, 1, 2, 3, 7, 21, 49, 165, 552, 2176, 9988, 46972,
    253293, 1388705, 8053393};

int myRank, numRanks;

struct Knot {
    int n = 0;
    bool alt = false;
    string diagram;
    string sig;
    string key;
};

// Which of two diagrams of the same knot to keep.
bool better(const Knot& a, const Knot& b) {
    if (a.n != b.n) return a.n < b.n;
    if (a.alt != b.alt) return a.alt;
    return a.diagram < b.diagram;
}

// ---------- MPI helpers ----------

double now() {
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime();
}

int owner(const string& key) {
    return hash<string>{}(key) % numRanks;
}

string pack(const Knot& k) {
    return to_string(k.n) + "\t" + (k.alt ? "1" : "0") + "\t" + k.diagram + "\t" + k.sig +
        "\t" + k.key;
}

Knot unpack(const string& line) {
    vector<string> parts;
    string part;
    stringstream in(line);
    while (getline(in, part, '\t')) parts.push_back(part);
    while (parts.size() < 5) parts.push_back("");
    Knot k;
    k.n = stoi(parts[0]);
    k.alt = parts[1] == "1";
    k.diagram = parts[2];
    k.sig = parts[3];
    k.key = parts[4];
    return k;
}

// Sends knots[i] to rank dest[i] and returns the knots this rank receives.
vector<Knot> sendTo(const vector<Knot>& knots, const vector<int>& dest) {
    vector<string> out(numRanks);
    for (size_t i = 0; i < knots.size(); i++)
        out[dest[i]] += pack(knots[i]) + "\n";

    string sendBuf;
    vector<int> sendCounts(numRanks), sendDispl(numRanks);
    for (int p = 0; p < numRanks; p++) {
        sendDispl[p] = sendBuf.size();
        sendCounts[p] = out[p].size();
        sendBuf += out[p];
    }
    if (sendBuf.size() > INT_MAX) {
        cerr << "too much data for one rank, use more ranks\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    vector<int> recvCounts(numRanks), recvDispl(numRanks);
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    long total = 0;
    for (int p = 0; p < numRanks; p++) {
        recvDispl[p] = total;
        total += recvCounts[p];
    }
    if (total > INT_MAX) {
        cerr << "too much data for one rank, use more ranks\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string recvBuf(total, ' ');
    MPI_Alltoallv(sendBuf.data(), sendCounts.data(), sendDispl.data(), MPI_CHAR,
        recvBuf.data(), recvCounts.data(), recvDispl.data(), MPI_CHAR, MPI_COMM_WORLD);

    vector<Knot> result;
    stringstream in(recvBuf);
    string line;
    while (getline(in, line))
        if (!line.empty()) result.push_back(unpack(line));
    return result;
}

// Every rank runs plantri and keeps every numRanks-th graph.
string myGraphs(int n, const string& plantri) {
    string command = plantri_command(n, 0, 1, plantri) + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw runtime_error("cannot run " + command);
    string keep, line;
    char buf[4096];
    long count = 0;
    while (fgets(buf, sizeof buf, pipe)) {
        line += buf;
        if (line.back() != '\n') continue;
        if (count % numRanks == myRank) keep += line;
        count++;
        line = "";
    }
    pclose(pipe);
    return keep;
}

// ---------- Regina helpers ----------

bool hasAngles(const Triangulation<3>& tri) {
    for (auto* v : tri.vertices())
        if (!v->isIdeal()) return false;
    return tri.hasStrictAngleStructure();
}

// Tries a few triangulations of the complement before the slow search.
bool certifyHyperbolic(const Knot& k) {
    Link knot = Link::fromSig(k.diagram);
    Triangulation<3> first = knot.complement();
    if (hasAngles(first)) return true;
    try {
        if (hasAngles(Triangulation<3>::fromIsoSig(k.sig))) return true;
    } catch (const exception&) {
    }
    SnapPeaTriangulation tri(knot);
    for (int i = 0; i < TRIES; i++) {
        tri.randomise();
        Triangulation<3> t(tri);
        t.simplify();
        if (hasAngles(t)) return true;
    }
    return regina_certify(first, "hyperbolic");
}

// Canonical signatures from random retriangulations of the same knot.
vector<string> moreSigs(const Knot& k) {
    vector<string> sigs = {k.sig};
    try {
        SnapPeaTriangulation tri(Link::fromSig(k.diagram));
        for (int i = 0; i < TRIES; i++) {
            try {
                tri.randomise();
                auto s = tri.solutionType();
                if (s == SnapPeaTriangulation::Solution::Geometric ||
                    s == SnapPeaTriangulation::Solution::Nongeometric)
                    sigs.push_back(tri.canonise().isoSig());
            } catch (const exception&) {
            }
        }
    } catch (const exception&) {
    }
    return sigs;
}

// Smallest signature near this triangulation. Equal results mean the same knot.
string minSig(Triangulation<3> tri) {
    tri.simplify();
    string best = tri.isoSig();
    tri.retriangulate(1, 1, nullptr, [&](const Triangulation<3>& t) {
        if (t.isoSig() < best) best = t.isoSig();
        return false;
    });
    return best;
}

// True if the two complements have a common triangulation nearby.
bool sameComplement(Triangulation<3> a, Triangulation<3> b) {
    a.simplify();
    b.simplify();
    if (a.isoSig() == b.isoSig()) return true;
    set<string> seen;
    a.retriangulate(1, 1, nullptr, [&](const Triangulation<3>& t) {
        seen.insert(t.isoSig());
        return false;
    });
    return b.retriangulate(1, 1, nullptr, [&](const Triangulation<3>& t) {
        return seen.count(t.isoSig()) > 0;
    });
}

// An alternating knot with n crossings has no diagram with n or fewer crossings
// that is not alternating, so the two must be different.
bool provenApart(const Knot& a, const Knot& b) {
    if (a.alt && b.alt) return true;
    if (a.alt && b.n <= a.n) return true;
    if (b.alt && a.n <= b.n) return true;
    return false;
}

bool shareSig(const vector<string>& a, const vector<string>& b) {
    for (const string& x : a)
        for (const string& y : b)
            if (x == y) return true;
    return false;
}

// ---------- Stages ----------

// 1. plantri graphs -> alternating knots and non-alternating diagrams.
vector<Knot> generateCandidates(int maxN, const string& plantri) {
    vector<Knot> all;
    for (int n = 3; n <= maxN; n++) {
        vector<AlternatingCandidate> graphs;
        stringstream in(myGraphs(n, plantri));
        gen_alternating(in, n, [&](AlternatingCandidate&& c) { graphs.push_back(move(c)); });

#pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < (int)graphs.size(); i++) {
            vector<Knot> found;
            try {
                if (flype_canonical(graphs[i].graph)) {
                    Link alt = graphs[i].alternating;
                    found.push_back({n, true, alt.sig(), snappyTri(alt, TRIES), ""});
                    switch_crossings(graphs[i].graph, [&](Link&& d) {
                        if (!randomR3Prefilter(d) || !reducingPassCheck(d)) return;
                        Link s(d);
                        s.simplifyToLocalMinimum();
                        if ((int)s.size() < n) return;
                        found.push_back({n, false, d.sig(), snappyTri(d, TRIES), ""});
                    });
                }
            } catch (const exception& e) {
                cerr << "skipped a graph: " << e.what() << '\n';
            }
#pragma omp critical
            all.insert(all.end(), found.begin(), found.end());
        }
    }
    return all;
}

// 2. Diagrams with the same complement signature are the same knot.
vector<Knot> removeDuplicates(const vector<Knot>& knots) {
    vector<int> dest;
    for (const Knot& k : knots) dest.push_back(owner(k.sig));
    vector<Knot> mine = sendTo(knots, dest);

    map<string, int> best;
    for (int i = 0; i < (int)mine.size(); i++) {
        if (!best.count(mine[i].sig) || better(mine[i], mine[best[mine[i].sig]]))
            best[mine[i].sig] = i;
    }
    vector<Knot> result;
    for (auto& [sig, i] : best) result.push_back(mine[i]);
    return result;
}

// Merges knots in a group that share a signature, then keeps the ones
// that are proven different from everything else in the group.
void mergeGroup(const vector<Knot>& knots, const vector<int>& group, vector<int>& finished,
    vector<int>& unresolved) {
    int g = group.size();
    vector<vector<string>> sigs(g);
#pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < g; i++) sigs[i] = moreSigs(knots[group[i]]);

    vector<int> label(g);
    for (int i = 0; i < g; i++) label[i] = i;
    for (int i = 0; i < g; i++) {
        for (int j = 0; j < i; j++) {
            if (label[i] != label[j] && shareSig(sigs[i], sigs[j])) {
                int old = label[i];
                for (int& l : label)
                    if (l == old) l = label[j];
            }
        }
    }

    map<int, int> best;
    for (int i = 0; i < g; i++) {
        if (!best.count(label[i]) || better(knots[group[i]], knots[group[best[label[i]]]]))
            best[label[i]] = i;
    }
    for (auto& [l, a] : best) {
        bool apart = true;
        for (auto& [l2, b] : best)
            if (a != b && !provenApart(knots[group[a]], knots[group[b]])) apart = false;
        if (apart) finished.push_back(group[a]);
        else unresolved.push_back(group[a]);
    }
}

// 3. Split by Jones polynomial, then by cover homology, then merge leftovers.
void splitByInvariants(vector<Knot>& hyp, vector<int>& finished, vector<int>& unresolved) {
#pragma omp parallel for schedule(dynamic, 16)
    for (int i = 0; i < (int)hyp.size(); i++)
        hyp[i].key = polynomial(Link::fromSig(hyp[i].diagram), false);

    vector<int> dest;
    for (const Knot& k : hyp) dest.push_back(owner(k.key));
    hyp = sendTo(hyp, dest);

    vector<int> everyone(hyp.size());
    iota(everyone.begin(), everyone.end(), 0);
    vector<vector<int>> groups = refine({everyone}, [&](int i) { return hyp[i].key; }, finished);

    for (int index = 3; index <= 6; index++) {
        vector<int> todo;
        for (auto& g : groups) todo.insert(todo.end(), g.begin(), g.end());
        vector<string> cover(hyp.size());
#pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < (int)todo.size(); i++)
            cover[todo[i]] = cover_homology(Link::fromSig(hyp[todo[i]].diagram), index, 6);
        groups = refine(groups, [&](int i) { return cover[i]; }, finished);
    }

    for (auto& g : groups) mergeGroup(hyp, g, finished, unresolved);
}

// 4. A strict angle structure proves a knot is hyperbolic, so prime.
//    Alternating knots are already proven by the flyping theorem.
void proveHyperbolic(const vector<Knot>& hyp, const vector<int>& finished,
    const vector<int>& unresolved, vector<long>& proven, vector<long>& open) {
    vector<int> ok(finished.size(), 0);
#pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < (int)finished.size(); i++) {
        const Knot& k = hyp[finished[i]];
        try {
            ok[i] = k.alt || certifyHyperbolic(k);
        } catch (const exception&) {
        }
    }
    for (int i = 0; i < (int)finished.size(); i++) {
        if (ok[i]) proven[hyp[finished[i]].n]++;
        else open[hyp[finished[i]].n]++;
    }
    for (int i : unresolved) open[hyp[i].n]++;
}

// 5. Non-hyperbolic knots are torus knots or satellites. Unknots, composites
//    and diagrams of smaller knots are dropped.
void handleNonHyperbolic(vector<Knot>& nonhyp, int maxN, vector<long>& proven,
    vector<long>& open) {
    vector<int> torusP, torusQ;
    vector<string> torusHomfly;
    for (int p = 2; p <= maxN; p++) {
        for (int q = p + 1; (p - 1) * q <= maxN; q++) {
            if (gcd(p, q) != 1) continue;
            Link t;
            t.insertTorusLink(p, q);
            torusP.push_back(p);
            torusQ.push_back(q);
            torusHomfly.push_back(polynomial(t, true));
        }
    }

#pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < (int)nonhyp.size(); i++) {
        Knot& k = nonhyp[i];
        k.key = "open";
        try {
            Link knot = Link::fromSig(k.diagram);
            if (!k.alt) {
                Link s(knot);
                s.simplifyExhaustive(1, 1);
                if ((int)s.size() < k.n) {
                    k.key = "drop";
                    continue;
                }
            }
            Triangulation<3> c = knot.complement();
            if (c.isSolidTorus()) {
                k.key = "drop";
                continue;
            }
            string homfly = polynomial(knot, true);
            for (int j = 0; j < (int)torusP.size(); j++) {
                if (homfly != torusHomfly[j]) continue;
                Link t;
                t.insertTorusLink(torusP[j], torusQ[j]);
                if (sameComplement(c, t.complement())) {
                    k.key = "torus " + to_string(torusP[j]) + "," + to_string(torusQ[j]);
                    break;
                }
            }
            if (k.key != "open") continue;
            if (regina_certify(c, "satellite")) {
                if (regina_certify(c, "prime")) k.key = "satellite " + homfly;
                else k.key = "drop";
                k.sig = minSig(c);
            }
        } catch (const exception&) {
        }
    }

    vector<Knot> keep;
    for (const Knot& k : nonhyp)
        if (k.key != "drop") keep.push_back(k);
    vector<int> dest(keep.size(), 0);
    vector<Knot> all = sendTo(keep, dest);
    if (myRank != 0) return;

    map<string, vector<int>> byKey;
    for (int i = 0; i < (int)all.size(); i++) {
        if (all[i].key == "open") open[all[i].n]++;
        else byKey[all[i].key].push_back(i);
    }
    for (auto& [key, members] : byKey) {
        // Torus knots with the same p,q are the same knot. Satellites are the
        // same knot if their signatures match.
        map<string, int> best;
        for (int i : members) {
            string id = key.rfind("torus", 0) == 0 ? key : all[i].sig;
            if (!best.count(id) || better(all[i], all[best[id]])) best[id] = i;
        }
        for (auto& [id, i] : best) {
            if (best.size() == 1) proven[all[i].n]++;
            else open[all[i].n]++;
        }
    }
}

// ---------- Main ----------

int main(int argc, char** argv) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    MPI_Comm_size(MPI_COMM_WORLD, &numRanks);

    if (argc < 2) {
        if (myRank == 0) cerr << "Usage: census MAX_CROSSINGS [--plantri PATH] [--csv FILE]\n";
        MPI_Finalize();
        return 1;
    }
    int maxN = stoi(argv[1]);
    string plantri = "plantri";
    string csv = "timing.csv";
    for (int i = 2; i + 1 < argc; i += 2) {
        if (string(argv[i]) == "--plantri") plantri = argv[i + 1];
        if (string(argv[i]) == "--csv") csv = argv[i + 1];
    }
    if (maxN < 3 || maxN > 17) {
        if (myRank == 0) cerr << "MAX_CROSSINGS must be between 3 and 17\n";
        MPI_Finalize();
        return 1;
    }

    vector<long> proven(maxN + 1, 0), open(maxN + 1, 0);
    double t0 = now();

    vector<Knot> candidates = generateCandidates(maxN, plantri);
    double t1 = now();

    vector<Knot> hyp, nonhyp;
    for (const Knot& k : candidates) {
        if (k.sig.empty()) nonhyp.push_back(k);
        else hyp.push_back(k);
    }
    candidates.clear();
    hyp = removeDuplicates(hyp);
    double t2 = now();

    vector<int> finished, unresolved;
    splitByInvariants(hyp, finished, unresolved);
    double t3 = now();

    proveHyperbolic(hyp, finished, unresolved, proven, open);
    double t4 = now();

    handleNonHyperbolic(nonhyp, maxN, proven, open);
    double t5 = now();

    vector<long> provenAll(maxN + 1), openAll(maxN + 1);
    MPI_Reduce(proven.data(), provenAll.data(), maxN + 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(open.data(), openAll.data(), maxN + 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    int status = 0;
    if (myRank == 0) {
        int threads = omp_get_max_threads();
        cout << "crossings proven unresolved expected match\n";
        for (int n = 3; n <= maxN; n++) {
            bool match = provenAll[n] == known[n] && openAll[n] == 0;
            if (!match) status = 1;
            cout << n << " " << provenAll[n] << " " << openAll[n] << " " << known[n] << " "
                 << (match ? "yes" : "NO") << '\n';
        }
        cout << (status == 0 ? "ALL MATCH" : "MISMATCH") << '\n';
        cout << "ranks=" << numRanks << " threads=" << threads << " seconds=" << t5 - t0 << '\n';

        bool newFile = !ifstream(csv).good();
        ofstream out(csv, ios::app);
        if (newFile)
            out << "max_crossings,ranks,threads,cores,total_s,generate_s,dedup_s,refine_s,"
                   "certify_s,nonhyperbolic_s\n";
        out << maxN << "," << numRanks << "," << threads << "," << numRanks * threads << ","
            << t5 - t0 << "," << t1 - t0 << "," << t2 - t1 << "," << t3 - t2 << ","
            << t4 - t3 << "," << t5 - t4 << '\n';
    }
    MPI_Finalize();
    return status;
}
