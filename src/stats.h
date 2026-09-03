#ifndef STATS_H
#define STATS_H

#include "evaluation.h"
#include "hierarchy.h"
#include "knn.h"

#include <cstdint>

struct RunStats {
    std::uint64_t input_dimension = 0;
    double parse_seconds = 0.0;
    double knn_seconds = 0.0;
    double probability_seconds = 0.0;
    double optimize_seconds = 0.0;
    double evaluation_seconds = 0.0;
    double output_seconds = 0.0;
    std::uint64_t peak_rss_bytes = 0;
    bool has_resolved_knn = false;
    KnnResolvedConfig resolved_knn;
    KnnStageStats knn_stage_stats;
    HierarchyStats hierarchy;
    bool has_evaluation = false;
    EvaluationResult evaluation;
};

#endif
