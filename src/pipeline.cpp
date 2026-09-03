#include "pipeline.h"

#include "io.h"
#include "evaluation.h"
#include "optimizer.h"
#include "output.h"
#include "probability_graph.h"
#include "stats.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

#ifdef __linux__
#include <sys/resource.h>
#endif

namespace {

double ElapsedSeconds(const std::chrono::steady_clock::time_point& begin) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
}

std::uint64_t PeakRssBytes() {
#ifdef __linux__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss > 0) {
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
    }
#endif
    return 0;
}

bool ParseUnsignedOption(const CliOptions& options,
                         const std::string& name,
                         std::uint64_t fallback,
                         std::uint64_t maximum,
                         std::uint64_t* value,
                         std::string* error) {
    if (!options.Has(name)) {
        *value = fallback;
        return true;
    }
    const std::string text = options.Get(name);
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (text.empty() || text[0] == '-' || errno == ERANGE || *end != '\0' || parsed == 0 || parsed > maximum) {
        *error = "Option --" + name + " must be a positive integer in range";
        return false;
    }
    *value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool ParseNonnegativeOption(const CliOptions& options,
                            const std::string& name,
                            std::uint64_t fallback,
                            std::uint64_t maximum,
                            std::uint64_t* value,
                            std::string* error) {
    if (!options.Has(name)) {
        *value = fallback;
        return true;
    }
    const std::string text = options.Get(name);
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (text.empty() || text[0] == '-' || errno == ERANGE || *end != '\0' || parsed > maximum) {
        *error = "Option --" + name + " must be a non-negative integer in range";
        return false;
    }
    *value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool ParseBooleanOption(const CliOptions& options,
                        const std::string& name,
                        bool fallback,
                        bool* value,
                        std::string* error) {
    if (!options.Has(name)) {
        *value = fallback;
        return true;
    }
    const std::string text = options.Get(name);
    if (text == "1" || text == "true") {
        *value = true;
        return true;
    }
    if (text == "0" || text == "false") {
        *value = false;
        return true;
    }
    *error = "Option --" + name + " accepts 0, 1, false, or true";
    return false;
}

bool ParseFloatOption(const CliOptions& options,
                      const std::string& name,
                      float fallback,
                      float minimum,
                      float maximum,
                      float* value,
                      std::string* error) {
    if (!options.Has(name)) {
        *value = fallback;
        return true;
    }
    const std::string text = options.Get(name);
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (text.empty() || errno == ERANGE || *end != '\0' ||
        !std::isfinite(parsed) || parsed < minimum || parsed > maximum) {
        *error = "Option --" + name + " must be a finite number in range";
        return false;
    }
    *value = parsed;
    return true;
}

bool BuildConfigs(const CliOptions& options,
                  ProbabilityGraphConfig* probability,
                  LayoutConfig* layout,
                  std::string* error) {
    std::uint64_t value = 0;
    if (!ParseUnsignedOption(options, "knn_k", probability->knn_k, 2047, &value, error)) return false;
    probability->knn_k = static_cast<std::uint32_t>(value);
    if (!ParseUnsignedOption(options, "threads", probability->threads, 1024, &value, error)) return false;
    probability->threads = static_cast<std::uint32_t>(value);
    layout->threads = probability->threads;
    if (!ParseNonnegativeOption(options, "seed", probability->seed,
                                std::numeric_limits<std::uint64_t>::max(), &value, error)) return false;
    probability->seed = value;
    layout->seed = value;
    if (!ParseUnsignedOption(options, "output_dim", layout->output_dimension, 64, &value, error)) return false;
    layout->output_dimension = static_cast<std::uint32_t>(value);
    if (!ParseNonnegativeOption(options, "epochs", layout->epochs, 100000, &value, error)) return false;
    layout->epochs = static_cast<std::uint32_t>(value);
    if (!ParseUnsignedOption(options, "samples", layout->samples, 1000000, &value, error)) return false;
    layout->samples = static_cast<std::uint32_t>(value);
    if (!ParseNonnegativeOption(options, "negative", layout->negative, 1000, &value, error)) return false;
    layout->negative = static_cast<std::uint32_t>(value);
    if (!ParseFloatOption(options, "alpha", layout->alpha, 0.000001f, 100.0f, &layout->alpha, error)) return false;
    if (!ParseFloatOption(options, "gamma", layout->gamma, 0.0f, 100.0f, &layout->gamma, error)) return false;
    if (!ParseUnsignedOption(options, "hierarchy_minimum_vertices", layout->hierarchy_minimum_vertices,
                             std::numeric_limits<std::uint32_t>::max(), &value, error)) return false;
    layout->hierarchy_minimum_vertices = static_cast<std::uint32_t>(value);
    if (!ParseBooleanOption(options, "deterministic", probability->deterministic,
                            &probability->deterministic, error)) return false;
    layout->deterministic = probability->deterministic;
    return true;
}

bool CheckOutputPaths(const CliOptions& options, std::string* error) {
    if (!options.Has("input")) {
        *error = "Missing --input";
        return false;
    }
    if (!options.validate_only && !options.Has("output")) {
        *error = "Missing --output";
        return false;
    }
    if (options.Has("output") && options.Has("stats_json") && options.Get("output") == options.Get("stats_json")) {
        *error = "--output and --stats-json must use different paths";
        return false;
    }
    if (options.Has("evaluate") && !options.Has("stats_json")) {
        *error = "--evaluate requires --stats-json";
        return false;
    }
    return true;
}

int Fail(const std::string& prefix, const std::string& error) {
    std::cerr << prefix << error << std::endl;
    return 2;
}

bool BuildEvaluationCohort(const DenseVectors& data,
                           std::uint64_t sample_vertices,
                           EvaluationCohort* cohort,
                           std::string* error) {
    const std::uint64_t sample_count = sample_vertices == 0 || sample_vertices >= data.point_count
        ? data.point_count : sample_vertices;
    if (sample_count > std::numeric_limits<std::size_t>::max() / data.dimension) {
        *error = "The high-dimensional evaluation sample is too large to allocate";
        return false;
    }
    cohort->dimension = data.dimension;
    cohort->vertex_ids.reserve(static_cast<std::size_t>(sample_count));
    cohort->values.reserve(static_cast<std::size_t>(sample_count * data.dimension));
    for (std::uint64_t sample = 0; sample < sample_count; ++sample) {
        const std::uint32_t vertex = static_cast<std::uint32_t>((sample * data.point_count) / sample_count);
        cohort->vertex_ids.push_back(vertex);
        const float* begin = data.values.data() + static_cast<std::size_t>(vertex * data.dimension);
        cohort->values.insert(cohort->values.end(), begin, begin + data.dimension);
    }
    return true;
}

bool ReadInput(const std::string& path,
               BinaryInputKind kind,
               DenseVectors* data,
               CsrGraph* graph,
               std::string* error) {
    if (kind == BinaryInputKind::Data) return ReadBinaryData(path, data, error);
    if (kind == BinaryInputKind::Graph) return ReadBinaryGraph(path, graph, error);
    *error = "The input extension must be .data or .graph";
    return false;
}

bool LoadLabels(const std::string& path,
                std::uint64_t vertex_count,
                std::vector<std::uint64_t>* labels,
                std::string* error) {
    std::ifstream input(path.c_str());
    if (!input) {
        *error = "Cannot open label file: " + path;
        return false;
    }
    labels->clear();
    labels->reserve(static_cast<std::size_t>(vertex_count));
    std::string token;
    while (input >> token) {
        char* end = nullptr;
        errno = 0;
        const unsigned long long value = std::strtoull(token.c_str(), &end, 10);
        if (token.empty() || token[0] == '-' || errno == ERANGE || *end != '\0') {
            *error = "Label file contains an invalid non-negative integer: " + token;
            return false;
        }
        if (labels->size() == static_cast<std::size_t>(vertex_count)) {
            *error = "Label file contains more labels than vertices";
            return false;
        }
        labels->push_back(static_cast<std::uint64_t>(value));
    }
    if (!input.eof()) {
        *error = "Cannot read label file: " + path;
        return false;
    }
    if (labels->size() != static_cast<std::size_t>(vertex_count)) {
        *error = "Label count does not match the number of vertices";
        return false;
    }
    return true;
}

}  // namespace

int RunPipeline(const CliOptions& options) {
    std::string error;
    if (!CheckOutputPaths(options, &error)) return Fail("Argument error: ", error);

    ProbabilityGraphConfig probability_config;
    LayoutConfig layout_config;
    if (!BuildConfigs(options, &probability_config, &layout_config, &error)) return Fail("Argument error: ", error);

    const std::string input = options.Get("input");
    const BinaryInputKind input_kind = DetectBinaryInputKind(input);
    std::uint64_t evaluation_sample_vertices = 0;
    if (options.Has("evaluate") && !ParseNonnegativeOption(options, "evaluation_sample_vertices", 0,
                                                            std::numeric_limits<std::uint32_t>::max(),
                                                            &evaluation_sample_vertices, &error)) {
        return Fail("Argument error: ", error);
    }
    std::uint64_t evaluation_neighborhood_hops = input_kind == BinaryInputKind::Graph ? 2 : 1;
    if (options.Has("evaluate") && !ParseUnsignedOption(options, "evaluation_neighborhood_hops",
                                                         evaluation_neighborhood_hops, 64,
                                                         &evaluation_neighborhood_hops, &error)) {
        return Fail("Argument error: ", error);
    }

    CsrGraph graph;
    DenseVectors data;
    std::vector<std::uint64_t> labels;
    RunStats stats;
    EvaluationCohort cohort;
    const std::chrono::steady_clock::time_point parse_begin = std::chrono::steady_clock::now();
    if (!ReadInput(input, input_kind, &data, &graph, &error)) return Fail("Input error: ", error);
    if (options.Has("labels")) {
        if (input_kind != BinaryInputKind::Data) {
            return Fail("Argument error: ", "--labels is only valid with .data input");
        }
        if (!LoadLabels(options.Get("labels"), data.point_count, &labels, &error)) {
            return Fail("Input error: ", error);
        }
    }
    stats.parse_seconds = ElapsedSeconds(parse_begin);
    if (options.validate_only) {
        if (data.point_count != 0) {
            graph.vertex_count = data.point_count;
            graph.row_offsets.assign(static_cast<std::size_t>(data.point_count + 1), 0);
            stats.input_dimension = data.dimension;
        }
        stats.peak_rss_bytes = PeakRssBytes();
        if (options.Has("stats_json") && !WriteStatsJson(options.Get("stats_json"), graph, nullptr,
                                                         probability_config, layout_config, stats, &error)) {
            return Fail("Output error: ", error);
        }
        std::cout << "Validation complete N=" << graph.vertex_count << " A=" << graph.weight.size() << std::endl;
        return 0;
    }

    ProbabilityGraphStats probability_stats;
    if (data.point_count != 0) {
        stats.input_dimension = data.dimension;
        if (options.Has("evaluate") && !BuildEvaluationCohort(data, evaluation_sample_vertices, &cohort, &error)) {
            return Fail("Run error: ", error);
        }
        if (!BuildProbabilityGraph(std::move(data), probability_config, &graph, &error, &probability_stats)) {
            return Fail("Run error: ", error);
        }
    } else {
        if (!BuildProbabilityGraph(&graph, probability_config, &error, &probability_stats)) {
            return Fail("Run error: ", error);
        }
    }
    stats.knn_seconds = probability_stats.knn_seconds;
    stats.probability_seconds = probability_stats.probability_seconds;
    stats.has_resolved_knn = probability_stats.has_resolved_knn;
    stats.resolved_knn = probability_stats.resolved_knn;
    stats.knn_stage_stats = probability_stats.knn_stage_stats;

    Embedding embedding;
    const std::chrono::steady_clock::time_point optimize_begin = std::chrono::steady_clock::now();
    if (!OptimizeLayout(&graph, layout_config, &embedding, &error, &stats.hierarchy)) {
        return Fail("Run error: ", error);
    }
    stats.optimize_seconds = ElapsedSeconds(optimize_begin);
    if (options.Has("evaluate")) {
        EvaluationConfig evaluation_config;
        evaluation_config.evaluation_sample_vertices = evaluation_sample_vertices;
        evaluation_config.neighborhood_hop_limit = static_cast<std::uint32_t>(evaluation_neighborhood_hops);
        const std::chrono::steady_clock::time_point evaluation_begin = std::chrono::steady_clock::now();
        if (!EvaluateLayout(graph, embedding, evaluation_config, &stats.evaluation, &error) ||
            (!cohort.vertex_ids.empty() && !EvaluateCohortTrustworthiness(
                cohort, embedding, probability_config.knn_k, &stats.evaluation, &error)) ||
            (!labels.empty() && !EvaluateClassification(
                embedding, labels, evaluation_sample_vertices, &stats.evaluation, &error))) {
            return Fail("Evaluation error: ", error);
        }
        stats.has_evaluation = true;
        stats.evaluation_seconds = ElapsedSeconds(evaluation_begin);
    }
    const std::chrono::steady_clock::time_point output_begin = std::chrono::steady_clock::now();
    if (!WriteEmbedding(options.Get("output"), embedding, &error)) return Fail("Output error: ", error);
    stats.output_seconds = ElapsedSeconds(output_begin);
    stats.peak_rss_bytes = PeakRssBytes();
    if (options.Has("stats_json") && !WriteStatsJson(options.Get("stats_json"), graph, &embedding,
                                                     probability_config, layout_config, stats, &error)) {
        return Fail("Output error: ", error);
    }
    std::cout << "Completed N=" << embedding.vertex_count << " output_dim=" << embedding.dimension << std::endl;
    return 0;
}
