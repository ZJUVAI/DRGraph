#ifndef OUTPUT_H
#define OUTPUT_H

#include "embedding.h"

#include <cstdint>
#include <string>

struct CsrGraph;
struct LayoutConfig;
struct ProbabilityGraphConfig;
struct RunStats;

bool WriteEmbedding(const std::string& path, const Embedding& embedding, std::string* error);
bool WriteStatsJson(const std::string& path,
                    const CsrGraph& graph,
                    const Embedding* embedding,
                    const ProbabilityGraphConfig& probability_config,
                    const LayoutConfig& layout_config,
                    const RunStats& stats,
                    std::string* error);

#endif
