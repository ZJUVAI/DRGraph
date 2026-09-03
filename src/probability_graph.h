#ifndef PROBABILITY_GRAPH_H
#define PROBABILITY_GRAPH_H

#include "graph.h"
#include "knn.h"

#include <cstdint>
#include <string>

struct ProbabilityGraphConfig {
    std::uint32_t knn_k = 15;
    std::uint32_t threads = 32;
    std::uint64_t seed = 1;
    bool deterministic = false;
};

struct ProbabilityGraphStats {
    std::uint64_t input_dimension = 0;
    double parse_seconds = 0.0;
    double knn_seconds = 0.0;
    double probability_seconds = 0.0;
    bool has_resolved_knn = false;
    KnnResolvedConfig resolved_knn;
    KnnStageStats knn_stage_stats;
};

bool BuildProbabilityGraph(DenseVectors data,
                           const ProbabilityGraphConfig& config,
                           CsrGraph* graph,
                           std::string* error,
                           ProbabilityGraphStats* stats = nullptr);

bool BuildProbabilityGraph(CsrGraph* graph,
                           const ProbabilityGraphConfig& config,
                           std::string* error,
                           ProbabilityGraphStats* stats = nullptr);

bool NormalizeGraphProbabilities(CsrGraph* graph, std::uint32_t threads, std::string* error);
bool NormalizeGraphWeights(CsrGraph* graph, std::uint32_t threads, std::string* error);

#endif
