// census MAX_CROSSINGS [--plantri PATH] [--csv FILE] [--tries N]
// Generates and proves every prime knot with 3 to MAX_CROSSINGS crossings.

#include "prime_knots/generate.hpp"
#include "prime_knots/prove.hpp"

#include <link/link.h>
#include <snappea/snappeatriangulation.h>
#include <triangulation/dim3.h>

#include <mpi.h>
#include <omp.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace prime_knots;
using regina::Link;
using regina::SnapPeaTriangulation;
using regina::Triangulation;

namespace {

// Prime knots with n crossings, up to mirror image (OEIS A002863).
const long known[] = {0, 0, 0, 1, 1, 2, 3, 7, 21, 49, 165, 552, 2176, 9988, 46972,
    253293, 1388705, 8053393};

int rank_, size_;

struct Knot {
    int n = 0;
    bool alt = false;
    string diagram;
    string sig;
    string key;
};

bool better(const Knot& a, const Knot& b) {
    if (a.n != b.n) return a.n < b.n;
    if (a.alt != b.alt) return a.alt;
    return a.diagram < b.diagram;
}

string pack(const Knot& k) {
    return to_string(k.n) + "\t" + (k.alt ? "1" : "0") + "\t" + k.diagram + "\t" + k.sig +
        "\t" + k.key;
}

Knot unpack(const string& line) {
    vector<string> f;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); i++) {
        if (i == line.size() || line[i] == '\t') {
            f.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    Knot k;
    k.n = stoi(f[0]);
    k.alt = f[1] == "1";
    k.diagram = f[2];
    k.sig = f[3];
    k.key = f[4];
    return k;
}

int owner(const string& key) {
    return (int)(hash<string>{}(key) % size_);
}

// Sends every knot to the rank that owns its key.
vector<Knot> sendTo(const vector<Knot>& knots, const function<int(const Knot&)>& dest) {
    vector<string> out(size_);
    for (const Knot& k : knots)
        out[dest(k)] += pack(k) + "\n";

    vector<int> sendCounts(size_), recvCounts(size_), sendDispl(size_), recvDispl(size_);
    string sendBuf;
    for (int p = 0; p < size_; p++) {
        if (out[p].size() > INT_MAX || sendBuf.size() + out[p].size() > INT_MAX) {
            cerr << "send too large, use more ranks\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        sendDispl[p] = (int)sendBuf.size();
        sendCounts[p] = (int)out[p].size();
        sendBuf += out[p];
    }
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    long long total = 0;
    for (int p = 0; p < size_; p++) {
        recvDispl[p] = (int)total;
        total += recvCounts[p];
    }
    if (total > INT_MAX) {
        cerr << "send too large, use more ranks\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    string recvBuf(total, '\0');
    MPI_Alltoallv(sendBuf.data(), sendCounts.data(), sendDispl.data(), MPI_CHAR,
        recvBuf.data(), recvCounts.data(), recvDispl.data(), MPI_CHAR, MPI_COMM_WORLD);

    vector<Knot> result;
    stringstream in(recvBuf);
    for (string line; getline(in, line);)
        if (!line.empty()) result.push_back(unpack(line));
    return result;
}

// Keeps one knot for each complement signature.
vector<Knot> dedup(const vector<Knot>& knots) {
    unordered_map<string, Knot> best;
    for (const Knot& k : knots) {
        auto it = best.find(k.sig);
        if (it == best.end()) best[k.sig] = k;
        else if (better(k, it->second)) it->second = k;
    }
    vector<Knot> result;
    for (auto& [sig, k] : best) result.push_back(k);
    return result;
}

vector<string> moreSigs(const Knot& k, int count) {
    vector<string> sigs = {k.sig};
    try {
        SnapPeaTriangulation tri(Link::fromSig(k.diagram));
        for (int i = 0; i < count; i++) {
            try {
                tri.randomise();
                auto s = tri.solutionType();
                if (s != SnapPeaTriangulation::Solution::Geometric &&
                    s != SnapPeaTriangulation::Solution::Nongeometric) continue;
                sigs.push_back(tri.canonise().isoSig());
            } catch (const exception&) {
            }
        }
    } catch (const exception&) {
    }
    return sigs;
}

bool quickAngles(const Triangulation<3>& tri) {
    for (const auto* v : tri.vertices())
        if (!v->isIdeal()) return false;
    return tri.hasStrictAngleStructure();
}

bool certifyHyperbolic(const Knot& k, int tries) {
    Link knot = Link::fromSig(k.diagram);
    Triangulation<3> first = knot.complement();
    if (quickAngles(first)) return true;
    try {
        if (quickAngles(Triangulation<3>::fromIsoSig(k.sig))) return true;
    } catch (const exception&) {
    }
    SnapPeaTriangulation tri(knot);
    for (int i = 0; i < tries; i++) {
        tri.randomise();
        Triangulation<3> t(tri);
        t.simplify();
        if (quickAngles(t)) return true;
    }
    return regina_certify(first, "hyperbolic");
}

// Smallest signature near this triangulation. Equal results mean the same knot.
string minSig(Triangulation<3> tri) {
    tri.simplify();
    string best = tri.isoSig();
    tri.retriangulate(1, 1, nullptr, [&](const Triangulation<3>& t) {
        string s = t.isoSig();
        if (s < best) best = s;
        return false;
    });
    return best;
}

bool sameComplement(Triangulation<3> a, Triangulation<3> b) {
    a.simplify();
    b.simplify();
    if (a.isoSig() == b.isoSig()) return true;
    set<string> seen;
    a.retriangulate(1, 1, nullptr, [&](const string& s, const Triangulation<3>&) {
        seen.insert(s);
        return false;
    });
    return b.retriangulate(1, 1, nullptr, [&](const string& s, const Triangulation<3>&) {
        return seen.count(s) > 0;
    });
}

// True if theory already says a and b are different knots.
bool provenApart(const Knot& a, const Knot& b) {
    if (a.alt && b.alt) return true;
    if (a.alt && b.n <= a.n) return true;
    if (b.alt && a.n <= b.n) return true;
    return false;
}

int root(vector<int>& parent, int x) {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
}

// Merges members of a group that are proven to be the same knot,
// then splits the result into finished knots and unresolved knots.
void settle(vector<Knot>& knots, const vector<int>& group, vector<vector<string>>& sigs,
    vector<int>& finished, vector<int>& unresolved) {
    int g = (int)group.size();
    vector<int> parent(g);
    iota(parent.begin(), parent.end(), 0);
    map<string, int> firstSeen;
    for (int i = 0; i < g; i++) {
        for (const string& s : sigs[i]) {
            auto it = firstSeen.find(s);
            if (it == firstSeen.end()) firstSeen[s] = i;
            else parent[root(parent, i)] = root(parent, it->second);
        }
    }
    map<int, int> bestOf;
    for (int i = 0; i < g; i++) {
        int r = root(parent, i);
        if (!bestOf.count(r) || better(knots[group[i]], knots[group[bestOf[r]]])) bestOf[r] = i;
    }
    vector<int> reps;
    for (auto& [r, i] : bestOf) reps.push_back(group[i]);
    for (int a : reps) {
        bool apart = true;
        for (int b : reps)
            if (a != b && !provenApart(knots[a], knots[b])) apart = false;
        if (apart) finished.push_back(a);
        else unresolved.push_back(a);
    }
}

struct Times {
    double generate = 0, dedup = 0, refine = 0, certify = 0, other = 0;
};

double now() {
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime();
}

// Every rank runs plantri and keeps every size-th graph, so the work is even.
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
        if (count % size_ == rank_) keep += line;
        count++;
        line.clear();
    }
    pclose(pipe);
    return keep;
}

} // namespace

int main(int argc, char** argv) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &size_);

    if (argc < 2) {
        if (rank_ == 0) cerr << "Usage: census MAX_CROSSINGS [--plantri PATH] [--csv FILE] [--tries N]\n";
        MPI_Finalize();
        return 1;
    }
    int maxN = stoi(argv[1]);
    string plantri = "plantri", csv = "timing.csv";
    int tries = 20;
    for (int i = 2; i + 1 < argc; i += 2) {
        string flag = argv[i];
        if (flag == "--plantri") plantri = argv[i + 1];
        else if (flag == "--csv") csv = argv[i + 1];
        else if (flag == "--tries") tries = stoi(argv[i + 1]);
    }
    if (maxN < 3 || maxN > 17) {
        if (rank_ == 0) cerr << "MAX_CROSSINGS must be between 3 and 17\n";
        MPI_Finalize();
        return 1;
    }
    int threads = omp_get_max_threads();
    Times t;
    double start = now(), mark = start;

    // 1. Generate candidates, filter them and find their complement signatures.
    vector<Knot> mine;
    for (int n = 3; n <= maxN; n++) {
        vector<AlternatingCandidate> graphs;
        stringstream in(myGraphs(n, plantri));
        gen_alternating(in, n, [&](AlternatingCandidate&& c) {
            graphs.push_back(move(c));
        });

#pragma omp parallel for schedule(dynamic, 1)
        for (long i = 0; i < (long)graphs.size(); i++) {
            vector<Knot> found;
            try {
                if (flype_canonical(graphs[i].graph)) {
                    const Link& alt = graphs[i].alternating;
                    found.push_back({n, true, alt.sig(), snappyTri(alt, tries), ""});
                    switch_crossings(graphs[i].graph, [&](Link&& d) {
                        if (!randomR3Prefilter(d) || !reducingPassCheck(d)) return;
                        Link s(d);
                        s.simplifyToLocalMinimum();
                        if ((int)s.size() < n) return;
                        found.push_back({n, false, d.sig(), snappyTri(d, tries), ""});
                    });
                }
            } catch (const exception& e) {
#pragma omp critical
                cerr << "rank " << rank_ << ": skipped graph: " << e.what() << '\n';
            }
#pragma omp critical
            mine.insert(mine.end(), found.begin(), found.end());
        }
    }
    double next = now();
    t.generate = next - mark;
    mark = next;

    // 2. Diagrams with the same complement are the same knot.
    vector<Knot> hyp, nonhyp;
    for (Knot& k : mine) (k.sig.empty() ? nonhyp : hyp).push_back(move(k));
    mine.clear();
    hyp = dedup(hyp);
    hyp = sendTo(hyp, [](const Knot& k) { return owner(k.sig); });
    hyp = dedup(hyp);
    next = now();
    t.dedup = next - mark;
    mark = next;

    // 3. Split by Jones polynomial, then by covers, and merge any that match.
#pragma omp parallel for schedule(dynamic, 16)
    for (long i = 0; i < (long)hyp.size(); i++)
        hyp[i].key = polynomial(Link::fromSig(hyp[i].diagram), false);
    hyp = sendTo(hyp, [](const Knot& k) { return owner(k.key); });

    vector<int> finished, unresolved;
    vector<vector<int>> groups;
    {
        vector<int> all(hyp.size());
        iota(all.begin(), all.end(), 0);
        groups = refine({all}, [&](int i) { return hyp[i].key; }, finished);
    }
    for (int k : {3, 4, 5, 6}) {
        vector<int> todo;
        for (auto& g : groups) todo.insert(todo.end(), g.begin(), g.end());
        vector<string> cover(hyp.size());
#pragma omp parallel for schedule(dynamic, 1)
        for (long i = 0; i < (long)todo.size(); i++)
            cover[todo[i]] = cover_homology(Link::fromSig(hyp[todo[i]].diagram), k, 6);
        groups = refine(groups, [&](int i) { return cover[i]; }, finished);
    }
    for (auto& g : groups) {
        vector<vector<string>> sigs(g.size());
#pragma omp parallel for schedule(dynamic, 1)
        for (long i = 0; i < (long)g.size(); i++)
            sigs[i] = moreSigs(hyp[g[i]], tries);
        settle(hyp, g, sigs, finished, unresolved);
    }
    next = now();
    t.refine = next - mark;
    mark = next;

    // 4. Prove the distinct knots are hyperbolic (or alternating), hence prime.
    vector<long> proven(maxN + 1, 0), open(maxN + 1, 0);
    vector<char> ok(finished.size(), 0);
#pragma omp parallel for schedule(dynamic, 1)
    for (long i = 0; i < (long)finished.size(); i++) {
        const Knot& k = hyp[finished[i]];
        try {
            ok[i] = k.alt || certifyHyperbolic(k, tries);
        } catch (const exception&) {
        }
    }
    for (size_t i = 0; i < finished.size(); i++)
        (ok[i] ? proven : open)[hyp[finished[i]].n]++;
    for (int i : unresolved) open[hyp[i].n]++;
    next = now();
    t.certify = next - mark;
    mark = next;

    // 5. Non-hyperbolic knots: torus knots, satellites, or diagrams to drop.
    vector<pair<int, int>> torus;
    vector<string> torusHomfly;
    for (int p = 2; p <= maxN; p++) {
        for (int q = p + 1; (p - 1) * q <= maxN; q++) {
            if (gcd(p, q) != 1) continue;
            Link tk;
            tk.insertTorusLink(p, q);
            torus.push_back({p, q});
            torusHomfly.push_back(polynomial(tk, true));
        }
    }

#pragma omp parallel for schedule(dynamic, 1)
    for (long i = 0; i < (long)nonhyp.size(); i++) {
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
            bool isTorus = false;
            for (size_t j = 0; j < torus.size() && !isTorus; j++) {
                if (homfly != torusHomfly[j]) continue;
                Link tk;
                tk.insertTorusLink(torus[j].first, torus[j].second);
                if (sameComplement(c, tk.complement())) {
                    k.key = "torus T(" + to_string(torus[j].first) + "," +
                        to_string(torus[j].second) + ")";
                    isTorus = true;
                }
            }
            if (isTorus) continue;
            if (regina_certify(c, "satellite")) {
                k.key = regina_certify(c, "prime") ? "satellite " + homfly : "drop";
                k.sig = minSig(c);
            }
        } catch (const exception&) {
        }
    }

    vector<Knot> keep;
    for (Knot& k : nonhyp)
        if (k.key != "drop") keep.push_back(k);
    nonhyp = sendTo(keep, [](const Knot&) { return 0; });
    if (rank_ == 0) {
        map<string, vector<int>> byKey;
        for (size_t i = 0; i < nonhyp.size(); i++) {
            if (nonhyp[i].key == "open") open[nonhyp[i].n]++;
            else byKey[nonhyp[i].key].push_back((int)i);
        }
        for (auto& [key, members] : byKey) {
            if (key[0] == 't') {
                int best = members[0];
                for (int i : members)
                    if (better(nonhyp[i], nonhyp[best])) best = i;
                proven[nonhyp[best].n]++;
                continue;
            }
            map<string, int> bestOf;
            for (int i : members) {
                auto it = bestOf.find(nonhyp[i].sig);
                if (it == bestOf.end()) bestOf[nonhyp[i].sig] = i;
                else if (better(nonhyp[i], nonhyp[it->second])) it->second = i;
            }
            for (auto& [sig, i] : bestOf)
                (bestOf.size() == 1 ? proven : open)[nonhyp[i].n]++;
        }
    }
    next = now();
    t.other = next - mark;
    double total = next - start;

    vector<long> provenAll(maxN + 1), openAll(maxN + 1);
    MPI_Reduce(proven.data(), provenAll.data(), maxN + 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(open.data(), openAll.data(), maxN + 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    int status = 0;
    if (rank_ == 0) {
        cout << "crossings proven unresolved expected match\n";
        for (int n = 3; n <= maxN; n++) {
            bool match = provenAll[n] == known[n] && openAll[n] == 0;
            if (!match) status = 1;
            cout << n << " " << provenAll[n] << " " << openAll[n] << " " << known[n] << " "
                 << (match ? "yes" : "NO") << '\n';
        }
        cout << (status == 0 ? "ALL MATCH" : "MISMATCH") << '\n';
        cout << "ranks=" << size_ << " threads=" << threads << " seconds=" << total << '\n';

        bool header = !ifstream(csv).good();
        ofstream out(csv, ios::app);
        if (header)
            out << "max_crossings,ranks,threads,cores,total_s,generate_s,dedup_s,refine_s,certify_s,"
                   "nonhyperbolic_s\n";
        out << maxN << "," << size_ << "," << threads << "," << size_ * threads << "," << total
            << "," << t.generate << "," << t.dedup << "," << t.refine << "," << t.certify << ","
            << t.other << '\n';
    }
    MPI_Finalize();
    return status;
}
