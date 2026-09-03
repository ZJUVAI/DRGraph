#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "embedding.h"
#include "graph.h"

#include <cstdint>
#include <string>
#include <vector>

struct HierarchyStats;

struct LayoutConfig {
    std::uint32_t output_dimension = 2;
    std::uint32_t epochs = 1;
    std::uint32_t samples = 400;
    std::uint32_t negative = 5;
    float alpha = 1.0f;
    float gamma = 0.1f;
    std::uint32_t threads = 32;
    std::uint32_t hierarchy_minimum_vertices = 100;
    std::uint64_t seed = 1;
    bool deterministic = false;
};

bool OptimizeLayout(CsrGraph* graph,
                    const LayoutConfig& config,
                    Embedding* embedding,
                    std::string* error,
                    HierarchyStats* hierarchy_stats = nullptr);
#endif
