#include "probability_graph.h"

#include "progress.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

#ifdef DRGRAPH_HAVE_OPENMP
#include <omp.h>
#endif

namespace {

bool CheckConfig(const ProbabilityGraphConfig& config, std::string* error) {
    if (config.knn_k == 0 || config.threads == 0) {
        *error = "kNN neighbor count and thread count must be positive";
        return false;
    }
    return true;
}

}  // namespace

bool NormalizeGraphProbabilities(CsrGraph* graph, std::uint32_t threads, std::string* error) {
    if (graph == nullptr || error == nullptr ||
        !ValidateCsrGraph(*graph, error, CsrWeightPolicy::AllowZero)) return false;
    if (threads == 0) {
        *error = "Probability-graph thread count must be positive";
        return false;
    }
    graph->uniform_edge_weights = false;
    ProgressBar::Begin("Compute probability graph", 4);
    std::vector<double> degree(static_cast<std::size_t>(graph->vertex_count), 0.0);
    bool invalid_degree = false;
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(threads) reduction(||:invalid_degree)
#endif
    for (std::int64_t row_index = 0; row_index < static_cast<std::int64_t>(degree.size()); ++row_index) {
        const std::size_t row = static_cast<std::size_t>(row_index);
        const std::size_t begin = static_cast<std::size_t>(graph->row_offsets[row]);
        const std::size_t end = static_cast<std::size_t>(graph->row_offsets[row + 1]);
        for (std::size_t edge = begin; edge < end; ++edge) degree[row] += graph->weight[edge];
        if (end > begin && (!std::isfinite(degree[row]) || degree[row] <= 0.0)) invalid_degree = true;
    }
    if (invalid_degree) {
        ProgressBar::Abort();
        *error = "Probability CSR row weights must have a positive finite sum";
        return false;
    }
    ProgressBar::Update(1);

    graph->negative_sampling_weight.resize(degree.size());
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(threads)
#endif
    for (std::int64_t row_index = 0; row_index < static_cast<std::int64_t>(degree.size()); ++row_index) {
        const std::size_t row = static_cast<std::size_t>(row_index);
        graph->negative_sampling_weight[row] = degree[row] > 0.0
            ? static_cast<float>(std::pow(degree[row], 0.75)) : 0.0f;
    }
    ProgressBar::Update(2);

    bool invalid_weight = false;
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(threads) reduction(||:invalid_weight)
#endif
    for (std::int64_t row_index = 0; row_index < static_cast<std::int64_t>(degree.size()); ++row_index) {
        const std::size_t row = static_cast<std::size_t>(row_index);
        const std::size_t begin = static_cast<std::size_t>(graph->row_offsets[row]);
        const std::size_t end = static_cast<std::size_t>(graph->row_offsets[row + 1]);
        for (std::size_t edge = begin; edge < end; ++edge) {
            const std::uint32_t target = graph->dst[edge];
            if (degree[row] == 0.0 || degree[target] == 0.0) continue;
            const double normalized = static_cast<double>(graph->weight[edge]) /
                std::sqrt(degree[row] * degree[target]);
            if (!std::isfinite(normalized) || normalized <= 0.0 ||
                normalized > std::numeric_limits<float>::max()) {
                invalid_weight = true;
                continue;
            }
            graph->weight[edge] = static_cast<float>(normalized);
        }
    }
    if (invalid_weight) {
        ProgressBar::Abort();
        *error = "Symmetric probability conversion produced an invalid edge weight";
        return false;
    }
    ProgressBar::Update(3);

    double total = 0.0;
    for (std::size_t edge = 0; edge < graph->weight.size(); ++edge) total += graph->weight[edge];
    if (!std::isfinite(total) || total <= 0.0) {
        ProgressBar::Abort();
        *error = "Probability CSR total weight must be positive and finite";
        return false;
    }
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(threads)
#endif
    for (std::int64_t edge_index = 0;
         edge_index < static_cast<std::int64_t>(graph->weight.size()); ++edge_index) {
        const std::size_t edge = static_cast<std::size_t>(edge_index);
        graph->weight[edge] = static_cast<float>(graph->weight[edge] / total);
    }
    ProgressBar::Finish();
    return true;
}

bool NormalizeGraphWeights(CsrGraph* graph, std::uint32_t threads, std::string* error) {
    if (graph == nullptr || error == nullptr ||
        !ValidateCsrGraph(*graph, error, CsrWeightPolicy::AllowZero)) return false;
    if (threads == 0) {
        *error = "Graph-weight thread count must be positive";
        return false;
    }
    ProgressBar::Begin("Normalize graph weights", 3);
    double total = 0.0;
    bool uniform_edge_weights = true;
    float first_weight = 0.0f;
    for (std::size_t edge = 0; edge < graph->weight.size(); ++edge) {
        const double transformed = std::exp(-std::min<double>(graph->weight[edge], 80.0));
        if (!std::isfinite(transformed) || transformed <= 0.0) {
            ProgressBar::Abort();
            *error = "Graph weight conversion produced an invalid value";
            return false;
        }
        graph->weight[edge] = static_cast<float>(transformed);
        if (edge == 0) first_weight = graph->weight[edge];
        else if (graph->weight[edge] != first_weight) uniform_edge_weights = false;
        total += transformed;
    }
    if (!std::isfinite(total) || total <= 0.0 || graph->vertex_count == 0) {
        ProgressBar::Abort();
        *error = "Graph total weight must be positive and finite";
        return false;
    }
    ProgressBar::Update(1);
    const double scale = static_cast<double>(graph->vertex_count) / total;
    for (std::size_t edge = 0; edge < graph->weight.size(); ++edge) {
        graph->weight[edge] = static_cast<float>(graph->weight[edge] * scale);
    }
    graph->uniform_edge_weights = uniform_edge_weights;
    graph->negative_sampling_weight.assign(static_cast<std::size_t>(graph->vertex_count), 0.0f);
    for (std::size_t row = 0; row < static_cast<std::size_t>(graph->vertex_count); ++row) {
        double degree = 0.0;
        for (std::size_t edge = static_cast<std::size_t>(graph->row_offsets[row]);
             edge < static_cast<std::size_t>(graph->row_offsets[row + 1]); ++edge) {
            degree += graph->weight[edge];
        }
        if (!std::isfinite(degree) || degree < 0.0) {
            ProgressBar::Abort();
        *error = "Graph weight sum must be finite";
            return false;
        }
        graph->negative_sampling_weight[row] = degree > 0.0
            ? static_cast<float>(std::pow(degree, 0.75)) : 0.0f;
    }
    ProgressBar::Update(2);
    ProgressBar::Finish();
    return true;
}

bool BuildProbabilityGraph(DenseVectors data,
                           const ProbabilityGraphConfig& config,
                           CsrGraph* graph,
                           std::string* error,
                           ProbabilityGraphStats* stats) {
    if (graph == nullptr || error == nullptr || !CheckConfig(config, error)) return false;
    if (stats != nullptr) *stats = ProbabilityGraphStats();

    if (stats != nullptr) stats->input_dimension = data.dimension;
    if (data.point_count < 2) {
        graph->vertex_count = data.point_count;
        graph->row_offsets.assign(static_cast<std::size_t>(data.point_count + 1), 0);
        graph->dst.clear();
        graph->weight.clear();
        graph->negative_sampling_weight.clear();
        return true;
    }
    KnnConfig knn_config;
    knn_config.neighbors = config.knn_k;
    knn_config.threads = config.threads;
    knn_config.seed = config.seed;
    knn_config.deterministic = config.deterministic;
    KnnResult knn;
    const std::chrono::steady_clock::time_point knn_begin = std::chrono::steady_clock::now();
    KnnResolvedConfig resolved;
    KnnStageStats knn_stats;
    if (!BuildKnn(data, knn_config, &knn, error, &resolved, &knn_stats)) return false;
    if (stats != nullptr) {
        stats->knn_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - knn_begin).count();
        stats->has_resolved_knn = true;
        stats->resolved_knn = resolved;
        stats->knn_stage_stats = knn_stats;
    }

    const std::uint64_t point_count = data.point_count;
    std::vector<float>().swap(data.values);
    std::vector<Edge> edges;
    edges.reserve(knn.indices.size());
    for (std::uint32_t source = 0; source < knn.point_count; ++source) {
        for (std::uint32_t neighbor = 0; neighbor < knn.neighbors; ++neighbor) {
            const std::size_t offset = static_cast<std::size_t>(source) * knn.neighbors + neighbor;
            const std::uint32_t target = knn.indices[offset];
            edges.push_back(Edge{std::min(source, target), std::max(source, target),
                                 std::exp(-std::min(knn.squared_distances[offset], 80.0f))});
        }
    }
    std::vector<std::uint32_t>().swap(knn.indices);
    std::vector<float>().swap(knn.squared_distances);
    const std::chrono::steady_clock::time_point probability_begin = std::chrono::steady_clock::now();
    if (!BuildUndirectedCsr(point_count, &edges, graph, error) ||
        !NormalizeGraphProbabilities(graph, config.threads, error)) return false;
    if (stats != nullptr) {
        stats->probability_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - probability_begin).count();
    }
    return true;
}

bool BuildProbabilityGraph(CsrGraph* graph,
                           const ProbabilityGraphConfig& config,
                           std::string* error,
                           ProbabilityGraphStats* stats) {
    if (graph == nullptr || error == nullptr || !CheckConfig(config, error)) return false;
    if (stats != nullptr) *stats = ProbabilityGraphStats();
    const std::chrono::steady_clock::time_point probability_begin = std::chrono::steady_clock::now();
    if (!NormalizeGraphWeights(graph, config.threads, error)) return false;
    if (stats != nullptr) {
        stats->probability_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - probability_begin).count();
    }
    return true;
}
