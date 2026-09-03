#ifndef EVALUATION_H
#define EVALUATION_H

#include "embedding.h"
#include "graph.h"

#include <cstdint>
#include <string>
#include <vector>

struct EvaluationCohort {
    std::uint64_t dimension = 0;
    std::vector<std::uint32_t> vertex_ids;
    std::vector<float> values;
};

struct EvaluationConfig {
    // Zero evaluates all vertices. A non-zero value limits deterministic sampling.
    std::uint64_t evaluation_sample_vertices = 0;
    // Neighborhood preservation uses this unweighted graph radius.
    std::uint32_t neighborhood_hop_limit = 1;
};

struct EvaluationResult {
    // Mean embedded distance over all directed CSR arcs.
    double average_directed_neighbor_distance = 0.0;
    // RMSE after fitting one scale to direct edges with target length one.
    double normalized_edge_stress = 0.0;
    // Macro-average neighborhood Jaccard.
    double neighborhood_jaccard = 0.0;
    // Number of non-isolated vertices included in neighborhood Jaccard.
    std::uint64_t evaluated_vertex_count = 0;
    // Number of directed CSR arcs included in edge metrics.
    std::uint64_t directed_edge_count = 0;
    // Availability flags are false when the corresponding population is empty.
    bool has_edge_metrics = false;
    bool has_neighborhood_jaccard = false;

    // Metrics restored from the original graph-layout evaluation program.
    double stress_neighbors = 0.0;
    double global_stress = 0.0;
    double kl_divergence = 0.0;
    std::uint64_t undirected_edge_count = 0;
    std::uint64_t global_stress_pair_count = 0;
    bool has_stress_neighbors = false;
    bool has_global_stress = false;
    bool has_kl_divergence = false;

    // Standard trustworthiness over the sampled high-dimensional cohort.
    double sampled_trustworthiness = 0.0;
    std::uint64_t trustworthiness_cohort_size = 0;
    std::uint32_t trustworthiness_k = 0;
    bool has_trustworthiness = false;

    // Original dimensionality-reduction classifier accuracies.
    std::uint64_t classification_sample_count = 0;
    std::uint32_t classification_training_count = 0;
    double accuracy_1nn = 0.0;
    double accuracy_5nn = 0.0;
    double accuracy_10nn = 0.0;
    double accuracy_20nn = 0.0;
    double accuracy_30nn = 0.0;
    double accuracy_40nn = 0.0;
    double accuracy_50nn = 0.0;
    bool has_classification = false;
};

bool EvaluateLayout(const CsrGraph& graph,
                           const Embedding& embedding,
                           const EvaluationConfig& config,
                           EvaluationResult* result,
                           std::string* error);

bool EvaluateCohortTrustworthiness(const EvaluationCohort& cohort,
                                   const Embedding& embedding,
                                   std::uint32_t requested_k,
                                   EvaluationResult* result,
                                   std::string* error);

bool EvaluateClassification(const Embedding& embedding,
                            const std::vector<std::uint64_t>& labels,
                            std::uint64_t requested_sample_vertices,
                            EvaluationResult* result,
                            std::string* error);

#endif
