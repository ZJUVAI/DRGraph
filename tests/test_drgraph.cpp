#include "graph.h"
#include "io.h"
#include "evaluation.h"
#include "hierarchy.h"
#include "knn.h"
#include "optimizer.h"
#include "probability_graph.h"
#include "sampling.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Failure: " << message << std::endl;
        ++failures;
    }
}

std::string Fixture(const char* name) {
    return std::string(TEST_FIXTURE_DIR) + "/" + name;
}

CsrGraph MakeCycleGraph(std::uint32_t vertex_count) {
    CsrGraph graph;
    graph.vertex_count = vertex_count;
    graph.row_offsets.resize(static_cast<std::size_t>(vertex_count) + 1);
    graph.dst.resize(static_cast<std::size_t>(vertex_count) * 2);
    graph.weight.assign(graph.dst.size(), 0.5f);
    graph.negative_sampling_weight.assign(vertex_count, 1.0f);
    for (std::uint32_t vertex = 0; vertex < vertex_count; ++vertex) {
        const std::size_t offset = static_cast<std::size_t>(vertex) * 2;
        graph.row_offsets[vertex] = offset;
        graph.dst[offset] = (vertex + vertex_count - 1) % vertex_count;
        graph.dst[offset + 1] = (vertex + 1) % vertex_count;
    }
    graph.row_offsets[vertex_count] = graph.dst.size();
    return graph;
}

void CheckProbabilityGraph(const CsrGraph& graph, const std::string& name) {
    Check(graph.row_offsets.size() == graph.vertex_count + 1, name + " CSR row offsets should be valid");
    Check(graph.row_offsets.back() == graph.weight.size(), name + " CSR end offset should be valid");
    double total = 0.0;
    for (std::size_t edge = 0; edge < graph.weight.size(); ++edge) {
        total += graph.weight[edge];
        Check(graph.dst[edge] < graph.vertex_count && std::isfinite(graph.weight[edge]) && graph.weight[edge] > 0.0f,
              name + " CSR edges should be valid");
    }
    Check(std::fabs(total - 1.0) < 1e-5, name + " probability weights should sum to one");
}

void CheckLayoutGraph(const CsrGraph& graph, const std::string& name) {
    double total = 0.0;
    for (std::size_t edge = 0; edge < graph.weight.size(); ++edge) {
        total += graph.weight[edge];
        Check(graph.dst[edge] < graph.vertex_count && std::isfinite(graph.weight[edge]) && graph.weight[edge] > 0.0f,
              name + " CSR edges should be valid");
    }
    Check(std::fabs(total - static_cast<double>(graph.vertex_count)) < 1e-4 * graph.vertex_count,
          name + " layout weights should sum close to the vertex count");
}

void TestInputAndCsr() {
    DenseVectors data;
    CsrGraph graph;
    std::string error;
    Check(ReadBinaryData(Fixture("pipeline.data"), &data, &error), "Binary .data should load: " + error);
    Check(data.point_count == 4 && data.dimension == 2, ".data shape should be valid");
    Check(ReadBinaryGraph(Fixture("duplicates.graph"), &graph, &error), "Binary .graph should load: " + error);
    Check(graph.vertex_count == 4 && !graph.weight.empty(), ".graph should build CSR");

    std::vector<Edge> edges;
    edges.push_back(Edge{1, 3, 2.0f});
    edges.push_back(Edge{3, 1, 0.5f});
    edges.push_back(Edge{0, 2, 1.0f});
    Check(BuildUndirectedCsr(4, &edges, &graph, &error), "Row-sort CSR should succeed: " + error);
    Check(edges.empty(), "Temporary edges should be released after CSR construction");
    Check(graph.row_offsets == std::vector<std::uint64_t>({0, 1, 2, 3, 4}), "CSR row offsets should be stable");
}

void TestCsrValidationPolicy() {
    CsrGraph graph;
    graph.vertex_count = 2;
    graph.row_offsets = {0, 1, 1};
    graph.dst = {1};
    graph.weight = {0.0f};
    std::string error;
    Check(ValidateCsrGraph(graph, &error, CsrWeightPolicy::AllowZero),
          "Shared CSR validation should allow zero weights when requested: " + error);
    Check(!ValidateCsrGraph(graph, &error, CsrWeightPolicy::RequirePositive),
          "Shared CSR validation should reject zero weights for positive graphs");
}

void TestExactKnnAndPipeline() {
    DenseVectors data;
    std::string error;
    Check(ReadBinaryData(Fixture("pipeline.data"), &data, &error), "Test vectors should load: " + error);
    KnnConfig knn_config;
    knn_config.neighbors = 2;
    knn_config.threads = 2;
    KnnResult knn;
    KnnResolvedConfig resolved;
    Check(BuildKnn(data, knn_config, &knn, &error, &resolved), "Exact kNN should complete on small data: " + error);
    Check(resolved.backend == "exact" && knn.indices.size() == 8, "Small data should use exact kNN");

    ProbabilityGraphConfig probability_config;
    probability_config.knn_k = 2;
    probability_config.threads = 2;
    probability_config.seed = 17;
    LayoutConfig layout_config;
    layout_config.output_dimension = 2;
    layout_config.epochs = 3;
    layout_config.negative = 1;
    layout_config.threads = 2;
    layout_config.seed = 17;
    layout_config.deterministic = true;
    CsrGraph graph;
    ProbabilityGraphStats stats;
    Check(BuildProbabilityGraph(std::move(data), probability_config, &graph, &error, &stats),
          ".data should converge to probability CSR: " + error);
    Check(stats.has_resolved_knn && stats.resolved_knn.backend == "exact", ".data stats should record exact kNN");
    CheckProbabilityGraph(graph, ".data");
    Embedding first;
    Embedding second;
    Check(OptimizeLayout(&graph, layout_config, &first, &error), "Optimization should complete: " + error);
    Check(OptimizeLayout(&graph, layout_config, &second, &error), "Repeated optimization should complete: " + error);
    Check(first.values == second.values, "Deterministic output should be stable");
    for (std::size_t index = 0; index < first.values.size(); ++index) {
        Check(std::isfinite(first.values[index]), "Embedding values should be finite");
    }
}

void TestKnnThreshold() {
    Check(UsesExactKnn(4095), "4095 vectors should use exact kNN");
    Check(!UsesExactKnn(4096), "4096 vectors should use EFANNA");
}

void TestDistanceLayout() {
    CsrGraph graph;
    graph.vertex_count = 2;
    graph.row_offsets = {0, 1, 2};
    graph.dst = {1, 0};
    graph.weight = {0.5f, 0.5f};

    LayoutConfig config;
    config.epochs = 1;
    config.negative = 0;
    config.alpha = 0.1f;
    config.threads = 1;
    config.seed = 23;
    config.hierarchy_minimum_vertices = 2;
    RandomGenerator random(config.seed + 0xa0761d6478bd642fULL);
    std::vector<float> initial(4);
    for (std::size_t index = 0; index < initial.size(); ++index) {
        initial[index] = (random.UnitFloat() - 0.5f) * 0.02f;
    }
    const float initial_x = initial[0] - initial[2];
    const float initial_y = initial[1] - initial[3];
    const float initial_distance = initial_x * initial_x + initial_y * initial_y;

    Embedding embedding;
    std::string error;
    Check(OptimizeLayout(&graph, config, &embedding, &error), "Distance layout should complete: " + error);
    const float final_x = embedding.values[0] - embedding.values[2];
    const float final_y = embedding.values[1] - embedding.values[3];
    const float final_distance = final_x * final_x + final_y * final_y;
    Check(final_distance < initial_distance, "Positive-edge updates should reduce node distance");
}

void TestGraphPipelineAndAliases() {
    ProbabilityGraphConfig probability_config;
    probability_config.threads = 2;
    LayoutConfig layout_config;
    layout_config.output_dimension = 2;
    layout_config.epochs = 3;
    layout_config.negative = 1;
    layout_config.threads = 2;
    CsrGraph graph;
    std::string error;
    Check(ReadBinaryGraph(Fixture("pipeline.graph"), &graph, &error),
          "Graph input should load: " + error);
    Check(BuildProbabilityGraph(&graph, probability_config, &error),
          ".graph should converge to probability CSR: " + error);
    CheckLayoutGraph(graph, ".graph");
    AliasTable negative;
    Check(BuildNegativeAliasTable(graph, &negative, &error), "Negative-sampling alias table should build: " + error);
    Check(negative.size() == graph.vertex_count && negative.alias.size() == graph.vertex_count,
          "Negative-sampling alias table should use O(N) space");

    CsrGraph uniform = MakeCycleGraph(16);
    AliasTable sources;
    AliasTable edges;
    Check(BuildPositiveAliasTables(uniform, &sources, &edges, &error),
          "Uniform graph positive sampling should build: " + error);
    Check(edges.probability.empty() && edges.alias.empty(),
          "Uniform graph positive sampling should avoid an edge-sized alias table");

    AliasTable weighted_sources;
    AliasTable weighted_edges;
    Check(BuildPositiveAliasTables(graph, &weighted_sources, &weighted_edges, &error),
          "Weighted graph positive sampling should build: " + error);
    Check(!weighted_edges.probability.empty() && weighted_edges.alias.size() == graph.weight.size(),
          "Weighted graph positive sampling should retain edge aliases");
}

void TestHierarchyAndFastLayout() {
    std::string error;
    Hierarchy hierarchy;
    Check(BuildHierarchy(MakeCycleGraph(64), 4, &hierarchy, &error, 31),
          "Hierarchy should build for a medium graph: " + error);
    Check(hierarchy.levels.size() > 1, "Hierarchy should contain a coarse level");
    for (std::size_t level = 0; level < hierarchy.levels.size(); ++level) {
        const HierarchyLevel& current = hierarchy.levels[level];
        Check(current.graph.row_offsets.size() == current.graph.vertex_count + 1,
              "Every hierarchy level should contain valid CSR offsets");
        if (level + 1 < hierarchy.levels.size()) {
            Check(current.fine_to_coarse.size() == current.graph.vertex_count,
                  "Fine-to-coarse mapping should cover every fine vertex");
            Check(current.fine_sun.size() == current.graph.vertex_count,
                  "Hierarchy anchors should cover every fine vertex");
            Check(current.interpolation_offsets.size() == current.graph.vertex_count + 1,
                  "Interpolation offsets should cover every fine vertex");
        }
    }

    CsrGraph graph = MakeCycleGraph(64);
    const std::vector<std::uint64_t> original_offsets = graph.row_offsets;
    const std::vector<std::uint32_t> original_dst = graph.dst;
    const std::vector<float> original_weight = graph.weight;
    LayoutConfig fast;
    fast.epochs = 4;
    fast.samples = 8;
    fast.negative = 2;
    fast.threads = 4;
    fast.hierarchy_minimum_vertices = 4;
    fast.seed = 37;
    HierarchyStats stats;
    Embedding embedding;
    Check(OptimizeLayout(&graph, fast, &embedding, &error, &stats),
          "Fast hierarchical layout should complete: " + error);
    Check(stats.used && stats.vertex_counts.size() > 1,
          "Fast layout should report hierarchy levels");
    Check(graph.row_offsets == original_offsets && graph.dst == original_dst && graph.weight == original_weight,
          "Optimization should restore the input graph after hierarchy processing");
    for (std::size_t index = 0; index < embedding.values.size(); ++index) {
        Check(std::isfinite(embedding.values[index]), "Fast layout coordinates should be finite");
    }

    LayoutConfig deterministic = fast;
    deterministic.deterministic = true;
    CsrGraph first_graph = MakeCycleGraph(64);
    CsrGraph second_graph = MakeCycleGraph(64);
    Embedding first;
    Embedding second;
    Check(OptimizeLayout(&first_graph, deterministic, &first, &error),
          "First deterministic hierarchical layout should complete: " + error);
    Check(OptimizeLayout(&second_graph, deterministic, &second, &error),
          "Second deterministic hierarchical layout should complete: " + error);
    Check(first.values == second.values, "Hierarchical deterministic output should be stable");
}

void TestInvalidBinary() {
    const std::string path = "drgraph_invalid_test.data";
    std::ofstream output(path.c_str(), std::ios::binary);
    output << "not-a-binary-input";
    output.close();
    DenseVectors data;
    std::string error;
    Check(!ReadBinaryData(path, &data, &error), "Text content should be rejected by binary input");
    std::remove(path.c_str());
}

void TestEvaluationMetrics() {
    CsrGraph graph;
    graph.vertex_count = 3;
    graph.row_offsets = {0, 2, 4, 6};
    graph.dst = {1, 2, 0, 2, 0, 1};
    graph.weight = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    Embedding embedding;
    embedding.vertex_count = 3;
    embedding.dimension = 1;
    embedding.values = {0.0f, 1.0f, 3.0f};
    EvaluationResult result;
    EvaluationConfig config;
    std::string error;
    Check(EvaluateLayout(graph, embedding, config, &result, &error),
          "Evaluation metrics should complete: " + error);
    Check(result.has_neighborhood_jaccard && std::fabs(result.neighborhood_jaccard - 1.0) < 1e-9,
          "Neighborhood Jaccard should be exact on the path fixture");
    Check(result.has_stress_neighbors && std::fabs(result.stress_neighbors - (1.0 / 7.0)) < 1e-9,
          "Neighbor stress should use the original mean-squared formula");
    Check(result.has_global_stress && result.global_stress_pair_count == 6,
          "Global stress should include every reachable directed pair");
    Check(result.has_global_stress && std::fabs(result.global_stress - (2.0 / 21.0)) < 1e-12,
          "Global stress should use the one-pass equivalent residual formula");
    Check(result.has_kl_divergence && std::isfinite(result.kl_divergence),
          "KL divergence should be finite");

    Embedding classification_embedding;
    classification_embedding.vertex_count = 6;
    classification_embedding.dimension = 1;
    classification_embedding.values = {0.0f, 0.1f, 0.2f, 10.0f, 10.1f, 10.2f};
    const std::vector<std::uint64_t> labels = {0, 0, 0, 1, 1, 1};
    EvaluationResult classification;
    Check(EvaluateClassification(classification_embedding, labels, 3, &classification, &error),
          "Classification evaluation should complete: " + error);
    Check(classification.has_classification && classification.classification_sample_count == 3,
          "Classification should report the requested holdout size");
    Check(std::fabs(classification.accuracy_1nn - 1.0) < 1e-9 &&
              std::fabs(classification.accuracy_5nn - (1.0 / 3.0)) < 1e-9,
          "Classification should report the original 1-NN and 5-NN behavior: " +
          std::to_string(classification.accuracy_1nn) + ", " + std::to_string(classification.accuracy_5nn));
}

}  // namespace

int main() {
    TestInputAndCsr();
    TestCsrValidationPolicy();
    TestExactKnnAndPipeline();
    TestKnnThreshold();
    TestDistanceLayout();
    TestGraphPipelineAndAliases();
    TestHierarchyAndFastLayout();
    TestInvalidBinary();
    TestEvaluationMetrics();
    if (failures != 0) {
        std::cerr << "Failure count: " << failures << std::endl;
        return 1;
    }
    std::cout << "All tests passed" << std::endl;
    return 0;
}
