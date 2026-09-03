#ifndef GRAPH_H
#define GRAPH_H

#include <cstdint>
#include <string>
#include <vector>

// Both input kinds are converted to one CSR graph here.
struct CsrGraph {
    std::uint64_t vertex_count = 0;
    std::vector<std::uint64_t> row_offsets;
    std::vector<std::uint32_t> dst;
    std::vector<float> weight;
    std::vector<float> negative_sampling_weight;
    // Set by graph normalization when every stored edge has the same weight.
    bool uniform_edge_weights = false;
};

struct Edge {
    std::uint32_t source;
    std::uint32_t target;
    float weight;
};

enum class CsrWeightPolicy {
    AllowZero,
    RequirePositive
};

// Validate the shared CSR representation at public stage boundaries.
bool ValidateCsrGraph(const CsrGraph& graph,
                      std::string* error,
                      CsrWeightPolicy weight_policy = CsrWeightPolicy::RequirePositive);

bool BuildUndirectedCsr(std::uint64_t vertex_count,
                        std::vector<Edge>* edges,
                        CsrGraph* graph,
                        std::string* error);

#endif
