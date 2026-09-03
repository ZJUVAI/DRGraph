#include "cli.h"

#include <sstream>

namespace {

struct OptionSpec {
    const char* canonical;
    const char* aliases[3];
};

const OptionSpec kOptions[] = {
    {"input", {"--input", "-input", nullptr}},
    {"output", {"--output", "-output", nullptr}},
    {"threads", {"--threads", "-threads", nullptr}},
    {"knn_k", {"--knn-k", "-knn_k", nullptr}},
    {"seed", {"--seed", nullptr, nullptr}},
    {"output_dim", {"--output-dim", nullptr, nullptr}},
    {"stats_json", {"--stats-json", nullptr, nullptr}},
    {"labels", {"--labels", nullptr, nullptr}},
    {"evaluation_sample_vertices", {"--evaluation-sample-vertices", nullptr, nullptr}},
    {"evaluation_neighborhood_hops", {"--evaluation-neighborhood-hops", nullptr, nullptr}},
    {"epochs", {"--epochs", nullptr, nullptr}},
    {"samples", {"--samples", nullptr, nullptr}},
    {"negative", {"--negative", nullptr, nullptr}},
    {"alpha", {"--alpha", nullptr, nullptr}},
    {"gamma", {"--gamma", nullptr, nullptr}},
    {"hierarchy_minimum_vertices", {"--hierarchy-minimum-vertices", nullptr, nullptr}},
};

const OptionSpec* FindOption(const std::string& name) {
    for (std::size_t option = 0; option < sizeof(kOptions) / sizeof(kOptions[0]); ++option) {
        for (std::size_t alias = 0; alias < 3 && kOptions[option].aliases[alias] != nullptr; ++alias) {
            if (name == kOptions[option].aliases[alias]) return &kOptions[option];
        }
    }
    return nullptr;
}

}  // namespace

bool CliOptions::Has(const std::string& name) const {
    return values.find(name) != values.end();
}

std::string CliOptions::Get(const std::string& name, const std::string& fallback) const {
    const std::map<std::string, std::string>::const_iterator value = values.find(name);
    return value == values.end() ? fallback : value->second;
}

bool ParseCli(int argc, char** argv, CliOptions* options, std::string* error) {
    CliOptions parsed;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            parsed.show_help = true;
            continue;
        }
        if (argument == "--help-all") {
            parsed.show_help_all = true;
            continue;
        }
        if (argument == "--validate-only") {
            parsed.validate_only = true;
            continue;
        }
        if (argument == "--evaluate") {
            parsed.values["evaluate"] = "1";
            continue;
        }
        if (argument == "--deterministic") {
            if (index + 1 < argc) {
                const std::string next(argv[index + 1]);
                if (next == "0" || next == "1" || next == "true" || next == "false") {
                    parsed.values["deterministic"] = next;
                    ++index;
                    continue;
                }
            }
            parsed.values["deterministic"] = "1";
            continue;
        }

        const std::size_t equals = argument.find('=');
        const std::string name = argument.substr(0, equals);
        const OptionSpec* spec = FindOption(name);
        if (spec == nullptr) {
            *error = "Unknown option: " + name + "; use --help for available options";
            return false;
        }
        std::string value;
        if (equals != std::string::npos) {
            value = argument.substr(equals + 1);
        } else if (index + 1 < argc) {
            value = argv[++index];
        } else {
            *error = "Option requires a value: " + name;
            return false;
        }
        if (value.empty()) {
            *error = "Option value is empty: " + name;
            return false;
        }
        parsed.values[spec->canonical] = value;
    }
    *options = parsed;
    return true;
}

std::string CliHelp(bool include_advanced) {
    std::ostringstream help;
    help << "Usage: drgraph --input INPUT --output OUTPUT [OPTIONS]\n"
         << "The .data extension selects dimensionality reduction. The .graph extension selects graph layout.\n"
         << "Inputs must use the DRGBIN01 little-endian binary format.\n\n"
         << "Common options:\n"
         << "  --input, -input PATH       Input .data or .graph file\n"
         << "  --output, -output PATH     Output coordinate file\n"
         << "  --threads, -threads COUNT  CPU worker count\n"
         << "  --knn-k, -knn_k COUNT      kNN neighbor count\n"
         << "  --seed VALUE               Random seed\n"
         << "  --output-dim COUNT         Output dimension\n"
         << "  --deterministic 0 or 1     Deterministic mode; default 0 (fast)\n"
         << "  --validate-only            Read and validate the input only\n"
         << "  --evaluate                 Write evaluation metrics to the stats JSON\n"
         << "  --labels PATH              One non-negative integer label per .data vertex\n"
         << "  --stats-json PATH          Write stage statistics\n"
         << "  --help                     Show common options\n"
         << "  --help-all                 Show advanced options\n";
    if (include_advanced) {
        help << "\nAdvanced options:\n"
            << "  --epochs COUNT                    Optimization epochs per level\n"
            << "  --samples COUNT                   Samples per vertex and epoch; default 400\n"
            << "  --negative COUNT                  Negative samples per positive edge\n"
            << "  --alpha VALUE                     Initial step size; default 1.0\n"
            << "  --gamma VALUE                     Fine-level negative-edge repulsion; default 0.1\n"
            << "  --hierarchy-minimum-vertices COUNT  Hierarchy stopping threshold\n"
            << "  --evaluation-sample-vertices COUNT  Evaluation vertices; 0 means all\n";
        help << "  --evaluation-neighborhood-hops COUNT  Graph hops used by neighborhood preservation\n";
    }
    return help.str();
}
