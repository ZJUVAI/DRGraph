#ifndef KNN_H
#define KNN_H

#include "vectors.h"

#include <cstdint>
#include <string>
#include <vector>

struct KnnConfig {
    std::uint32_t neighbors = 15;
    std::uint32_t threads = 1;
    std::uint64_t seed = 1;
    bool deterministic = false;
};

struct KnnResult {
    std::uint64_t point_count = 0;
    std::uint32_t neighbors = 0;
    std::vector<std::uint32_t> indices;
    std::vector<float> squared_distances;
};

struct KnnResolvedConfig {
    std::string backend;
    std::uint32_t neighbors = 0;
    std::uint32_t threads = 0;
    bool deterministic = false;
    std::string efanna_profile;
};

struct KnnStageStats {
    double index_build_seconds = 0.0;
    double search_seconds = 0.0;
    double result_finalize_seconds = 0.0;
};

bool UsesExactKnn(std::uint64_t point_count);

bool BuildKnn(const DenseVectors& data,
              const KnnConfig& config,
              KnnResult* result,
              std::string* error,
              KnnResolvedConfig* resolved_config = nullptr,
              KnnStageStats* stage_stats = nullptr);

#endif
