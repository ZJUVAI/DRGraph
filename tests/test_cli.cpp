#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Failure: " << message << std::endl;
        ++failures;
    }
}

bool ExistsAndNotEmpty(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    return input && input.tellg() > 0;
}

bool Contains(const std::string& path, const std::string& needle) {
    std::ifstream input(path.c_str());
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return content.find(needle) != std::string::npos;
}

bool SameContent(const std::string& left, const std::string& right) {
    std::ifstream left_input(left.c_str(), std::ios::binary);
    std::ifstream right_input(right.c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(left_input)), std::istreambuf_iterator<char>()) ==
           std::string((std::istreambuf_iterator<char>(right_input)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    const std::string input = std::string(TEST_FIXTURE_DIR) + "/pipeline.graph";
    const std::string output = "drgraph_cli_test.embedding";
    const std::string stats = "drgraph_cli_test.json";
    const std::string command = std::string(DRGRAPH_PROGRAM_PATH) + " --input " + input +
        " --output " + output + " --stats-json " + stats + " --evaluate" +
        " --epochs 2 --negative 1 --threads 1 --hierarchy-minimum-vertices 1";
    Check(std::system(command.c_str()) == 0, "Graph-layout CLI should succeed");
    Check(ExistsAndNotEmpty(output) && ExistsAndNotEmpty(stats), "CLI should write embedding and stats JSON");
    Check(Contains(stats, "\"deterministic\": false"), "CLI should default to fast optimization");
    Check(Contains(stats, "neighborhood_preservation") && Contains(stats, "stress_neighbors") &&
              Contains(stats, "global_stress") && Contains(stats, "kl_divergence"),
          "Stats JSON should contain all graph-layout metrics");
    Check(Contains(stats, "\"vertex_counts\": [") && Contains(stats, "\"arc_counts\": ["),
          "Stats JSON should describe hierarchy sizes");
    const std::string data_output = "drgraph_cli_data_test.embedding";
    const std::string data_stats = "drgraph_cli_data_test.json";
    const std::string data_command = std::string(DRGRAPH_PROGRAM_PATH) + " --input " +
        std::string(TEST_FIXTURE_DIR) + "/pipeline.data --output " + data_output +
        " --stats-json " + data_stats + " --evaluate --labels " +
        std::string(TEST_FIXTURE_DIR) + "/pipeline.labels --epochs 2 --negative 0 --threads 1";
    Check(std::system(data_command.c_str()) == 0, "Data-reduction CLI should succeed");
    Check(Contains(data_stats, "accuracy_1nn") && Contains(data_stats, "accuracy_50nn"),
          "Stats JSON should contain classifier metrics");
    const std::string removed_option_command = std::string(DRGRAPH_PROGRAM_PATH) +
        " --input " + input + " --output ignored.embedding --unknown-option value";
    Check(std::system(removed_option_command.c_str()) != 0,
          "Unknown options should be rejected");
    const std::string legacy_command = std::string(DRGRAPH_PROGRAM_PATH) + " --input " + input +
        " --output ignored.embedding --knn-backend exact";
    Check(std::system(legacy_command.c_str()) != 0, "The legacy kNN backend option should be rejected");
    const std::string deterministic_first = "drgraph_cli_deterministic_first.embedding";
    const std::string deterministic_second = "drgraph_cli_deterministic_second.embedding";
    const std::string deterministic_command = std::string(DRGRAPH_PROGRAM_PATH) + " --input " + input +
        " --epochs 3 --samples 20 --negative 2 --threads 4 --seed 19 --deterministic --output ";
    Check(std::system((deterministic_command + deterministic_first).c_str()) == 0 &&
              std::system((deterministic_command + deterministic_second).c_str()) == 0,
          "Repeated deterministic CLI runs should succeed");
    Check(SameContent(deterministic_first, deterministic_second),
          "Repeated deterministic CLI runs should write identical embeddings");
    const std::string unknown_command = std::string(DRGRAPH_PROGRAM_PATH) +
        " --input unknown.txt --output ignored.embedding";
    Check(std::system(unknown_command.c_str()) != 0, "Unknown input extensions should be rejected");
    std::remove(output.c_str());
    std::remove(stats.c_str());
    std::remove(data_output.c_str());
    std::remove(data_stats.c_str());
    std::remove(deterministic_first.c_str());
    std::remove(deterministic_second.c_str());
    std::remove("ignored.embedding");
    if (failures != 0) return 1;
    std::cout << "CLI tests passed" << std::endl;
    return 0;
}
