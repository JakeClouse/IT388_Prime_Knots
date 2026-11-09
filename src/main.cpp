// prime-knots INPUT [--threads N]
// Classifies every knot diagram in INPUT and prints one JSON line per diagram.

#include "prime_knots/prime_knots.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace prime_knots;
    if (argc != 2 && !(argc == 4 && std::string(argv[2]) == "--threads")) {
        std::cerr << "Usage: prime-knots INPUT [--threads N]\n"
                  << "INPUT has one diagram per line: a name, then 4 PD labels per crossing.\n";
        return 1;
    }
    int threads = 1;
    if (argc == 4) {
        try {
            threads = std::stoi(argv[3]);
        } catch (const std::exception&) {
            threads = 0;
        }
        if (threads < 1) {
            std::cerr << "--threads must be a positive integer\n";
            return 1;
        }
#ifndef _OPENMP
        if (threads > 1) {
            std::cerr << "built without OpenMP; use --threads 1\n";
            return 1;
        }
#endif
    }

    // 1. Read the input, skipping blank lines and # comments.
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "cannot open " << argv[1] << '\n';
        return 1;
    }
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);) {
        const auto start = line.find_first_not_of(" \t\r");
        if (start != std::string::npos && line[start] != '#') lines.push_back(line);
    }

    // 2. Classify. Each diagram is independent, so threads share the list;
    //    "dynamic" hands out one diagram at a time because costs vary a lot.
    std::vector<Report> reports(lines.size());
    const auto start = std::chrono::steady_clock::now();
#pragma omp parallel for schedule(dynamic, 1) num_threads(threads)
    for (long i = 0; i < static_cast<long>(lines.size()); ++i)
        reports[static_cast<std::size_t>(i)] = classify(lines[static_cast<std::size_t>(i)]);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

    // 3. Print results in input order, then a summary on stderr.
    std::map<std::string, int> counts;
    for (const auto& report : reports) {
        std::cout << to_json(report) << '\n';
        ++counts[to_string(report.status)];
    }
    std::cerr << "diagrams=" << reports.size() << " threads=" << threads
              << " seconds=" << elapsed.count();
    for (const auto& [status, count] : counts) std::cerr << ' ' << status << '=' << count;
    std::cerr << '\n';
    return 0;
}
