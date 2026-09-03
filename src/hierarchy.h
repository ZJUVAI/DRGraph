#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "graph.h"

#include <cstdint>
#include <string>
#include <vector>

struct HierarchyLevel {
    CsrGraph graph;
    std::vector<std::uint32_t> fine_to_coarse;
    std::vector<std::uint32_t> fine_sun;
    std::vector<std::uint64_t> interpolation_offsets;
    std::vector<std::uint32_t> interpolation_suns;
    std::vector<float> interpolation_lambda;
};

struct Hierarchy {
    std::vector<HierarchyLevel> levels;
};

struct HierarchyStats {
    bool used = false;
    double build_seconds = 0.0;
    std::vector<std::uint64_t> vertex_counts;
    std::vector<std::uint64_t> arc_counts;
    std::uint32_t epochs_per_level = 0;
};

bool BuildHierarchy(CsrGraph&& graph,
                    std::uint32_t minimum_coarse_vertices,
                    Hierarchy* hierarchy,
                    std::string* error,
                    std::uint64_t seed = 1);

#endif
