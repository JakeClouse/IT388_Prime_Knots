// Reading input lines and writing results. No Regina here.

#include "prime_knots/prime_knots.hpp"

#include <sstream>
#include <stdexcept>

namespace prime_knots {

Candidate parse_candidate(const std::string& line) {
    std::istringstream in(line);
    Candidate candidate;
    if (!(in >> candidate.id)) throw std::invalid_argument("empty line");

    // Read every remaining token as an integer arc label.
    std::vector<int> labels;
    std::string token;
    while (in >> token) {
        std::size_t used = 0;
        int label = 0;
        try {
            label = std::stoi(token, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (used == 0 || used != token.size())
            throw std::invalid_argument("not an integer: " + token);
        labels.push_back(label);
    }
    if (labels.size() % 4 != 0)
        throw std::invalid_argument("each crossing needs exactly 4 labels");
    for (std::size_t i = 0; i < labels.size(); i += 4)
        candidate.pd.push_back({labels[i], labels[i + 1], labels[i + 2], labels[i + 3]});

    // n crossings have 2n arcs: every label 1..2n must appear exactly twice.
    const std::size_t arcs = 2 * candidate.pd.size();
    std::vector<int> seen(arcs + 1, 0);
    for (int label : labels) {
        if (label < 1 || static_cast<std::size_t>(label) > arcs)
            throw std::invalid_argument("labels must be between 1 and 2n");
        ++seen[static_cast<std::size_t>(label)];
    }
    for (std::size_t label = 1; label <= arcs; ++label)
        if (seen[label] != 2) throw std::invalid_argument("every label must appear exactly twice");
    return candidate;
}

std::string to_string(Status status) {
    switch (status) {
        case Status::prime_alternating: return "PRIME_ALTERNATING";
        case Status::prime_hyperbolic: return "PRIME_HYPERBOLIC";
        case Status::not_minimal: return "NOT_MINIMAL";
        case Status::composite: return "COMPOSITE";
        case Status::unresolved: return "UNRESOLVED";
        case Status::invalid_input: return "INVALID_INPUT";
    }
    return "UNKNOWN";
}

namespace {
// JSON string with quotes, backslashes and control characters escaped.
std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (const char character : text) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20) {
            const char* hex = "0123456789abcdef";
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 0xf];
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}
} // namespace

std::string to_json(const Report& report) {
    return "{\"id\":" + quoted(report.id) +
        ",\"crossings\":" + std::to_string(report.crossings) +
        ",\"status\":" + quoted(to_string(report.status)) +
        ",\"reason\":" + quoted(report.reason) +
        ",\"signature\":" + quoted(report.signature) + "}";
}

} // namespace prime_knots
