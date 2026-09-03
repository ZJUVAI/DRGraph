#include "output.h"

#include "optimizer.h"
#include "probability_graph.h"
#include "stats.h"

#include <cmath>
#include <cstdio>

bool WriteEmbedding(const std::string& path, const Embedding& embedding, std::string* error) {
    const std::string temporary_path = path + ".partial";
    FILE* file = std::fopen(temporary_path.c_str(), "w");
    if (file == nullptr) {
        *error = "Cannot create output file: " + temporary_path;
        return false;
    }
    bool valid = std::fprintf(file, "%llu %u\n", static_cast<unsigned long long>(embedding.vertex_count), embedding.dimension) > 0;
    for (std::size_t row = 0; valid && row < static_cast<std::size_t>(embedding.vertex_count); ++row) {
        for (std::uint32_t dimension = 0; dimension < embedding.dimension; ++dimension) {
            const float value = embedding.values[row * embedding.dimension + dimension];
            valid = std::isfinite(value) && std::fprintf(file, dimension == 0 ? "%.9g" : " %.9g", value) > 0;
            if (!valid) break;
        }
        valid = valid && std::fprintf(file, "\n") > 0;
    }
    valid = valid && std::fclose(file) == 0;
    if (!valid || std::rename(temporary_path.c_str(), path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        *error = "Cannot write embedding output completely: " + path;
        return false;
    }
    return true;
}

bool WriteStatsJson(const std::string& path,
                    const CsrGraph& graph,
                    const Embedding* embedding,
                    const ProbabilityGraphConfig& probability_config,
                    const LayoutConfig& layout_config,
                    const RunStats& stats,
                    std::string* error) {
    const std::string temporary_path = path + ".partial";
    FILE* file = std::fopen(temporary_path.c_str(), "w");
    if (file == nullptr) {
        *error = "Cannot create stats file: " + temporary_path;
        return false;
    }
    const std::uint64_t arcs = graph.weight.size();
    bool valid = std::fprintf(file,
        "{\n  \"N\": %llu, \"D\": %llu, \"A\": %llu,\n"
        "  \"parse_seconds\": %.9f,\n  \"knn_seconds\": %.9f,\n"
        "  \"probability_seconds\": %.9f,\n  \"optimize_seconds\": %.9f,\n"
        "  \"evaluation_seconds\": %.9f,\n  \"output_seconds\": %.9f,\n"
        "  \"peak_rss_bytes\": %llu,\n"
        "  \"configuration\": {\"threads\": %u, \"seed\": %llu, \"deterministic\": %s, \"knn_k\": %u, \"epochs\": %u, \"samples\": %u, \"negative\": %u, \"alpha\": %.9g, \"gamma\": %.9g, \"hierarchy_minimum_vertices\": %u}",
        static_cast<unsigned long long>(graph.vertex_count),
        static_cast<unsigned long long>(stats.input_dimension),
        static_cast<unsigned long long>(arcs), stats.parse_seconds, stats.knn_seconds,
        stats.probability_seconds, stats.optimize_seconds, stats.evaluation_seconds,
        stats.output_seconds, static_cast<unsigned long long>(stats.peak_rss_bytes), probability_config.threads,
        static_cast<unsigned long long>(probability_config.seed),
        probability_config.deterministic ? "true" : "false", probability_config.knn_k,
        layout_config.epochs, layout_config.samples, layout_config.negative, layout_config.alpha, layout_config.gamma,
        layout_config.hierarchy_minimum_vertices) > 0;
    if (stats.has_resolved_knn) {
        valid = valid && std::fprintf(file,
            ",\n  \"resolved_knn\": {\"backend\": \"%s\", \"neighbors\": %u, \"threads\": %u, \"deterministic\": %s, \"profile\": \"%s\"}",
            stats.resolved_knn.backend.c_str(), stats.resolved_knn.neighbors,
            stats.resolved_knn.threads, stats.resolved_knn.deterministic ? "true" : "false",
            stats.resolved_knn.efanna_profile.c_str()) > 0;
        valid = valid && std::fprintf(file,
            ",\n  \"knn_stages\": {\"index_build_seconds\": %.9f, \"search_seconds\": %.9f, \"result_finalize_seconds\": %.9f}",
            stats.knn_stage_stats.index_build_seconds, stats.knn_stage_stats.search_seconds,
            stats.knn_stage_stats.result_finalize_seconds) > 0;
    }
    if (stats.hierarchy.used) {
        valid = valid && std::fprintf(file,
            ",\n  \"hierarchy\": {\"build_seconds\": %.9f, \"epochs_per_level\": %u, \"vertex_counts\": [",
            stats.hierarchy.build_seconds, stats.hierarchy.epochs_per_level) > 0;
        for (std::size_t level = 0; valid && level < stats.hierarchy.vertex_counts.size(); ++level) {
            valid = std::fprintf(file, level == 0 ? "%llu" : ", %llu",
                static_cast<unsigned long long>(stats.hierarchy.vertex_counts[level])) > 0;
        }
        valid = valid && std::fprintf(file, "], \"arc_counts\": [") > 0;
        for (std::size_t level = 0; valid && level < stats.hierarchy.arc_counts.size(); ++level) {
            valid = std::fprintf(file, level == 0 ? "%llu" : ", %llu",
                static_cast<unsigned long long>(stats.hierarchy.arc_counts[level])) > 0;
        }
        valid = valid && std::fprintf(file, "]}") > 0;
    }
    if (embedding != nullptr) valid = valid && std::fprintf(file, ",\n  \"output_dim\": %u", embedding->dimension) > 0;
    if (stats.has_evaluation) {
        valid = valid && std::fprintf(file,
            ",\n  \"evaluation\": {\n"
            "    \"average_directed_neighbor_distance\": %.9f,\n"
            "    \"normalized_edge_stress\": %.9f,\n"
            "    \"neighborhood_jaccard\": %.9f,\n"
            "    \"neighborhood_preservation\": %.9f,\n"
            "    \"stress_neighbors\": %.9f,\n"
            "    \"global_stress\": %.9f,\n"
            "    \"kl_divergence\": %.9f,\n"
            "    \"evaluated_vertex_count\": %llu,\n"
            "    \"directed_edge_count\": %llu,\n"
            "    \"undirected_edge_count\": %llu,\n"
            "    \"global_stress_pair_count\": %llu,\n"
            "    \"available\": {\"edge_metrics\": %s, \"neighborhood_jaccard\": %s, \"stress_neighbors\": %s, \"global_stress\": %s, \"kl_divergence\": %s, \"trustworthiness\": %s, \"classification\": %s},\n"
            "    \"trustworthiness\": ",
            stats.evaluation.average_directed_neighbor_distance, stats.evaluation.normalized_edge_stress,
            stats.evaluation.neighborhood_jaccard, stats.evaluation.neighborhood_jaccard,
            stats.evaluation.stress_neighbors,
            stats.evaluation.global_stress, stats.evaluation.kl_divergence,
            static_cast<unsigned long long>(stats.evaluation.evaluated_vertex_count),
            static_cast<unsigned long long>(stats.evaluation.directed_edge_count),
            static_cast<unsigned long long>(stats.evaluation.undirected_edge_count),
            static_cast<unsigned long long>(stats.evaluation.global_stress_pair_count),
            stats.evaluation.has_edge_metrics ? "true" : "false",
            stats.evaluation.has_neighborhood_jaccard ? "true" : "false",
            stats.evaluation.has_stress_neighbors ? "true" : "false",
            stats.evaluation.has_global_stress ? "true" : "false",
            stats.evaluation.has_kl_divergence ? "true" : "false",
            stats.evaluation.has_trustworthiness ? "true" : "false",
            stats.evaluation.has_classification ? "true" : "false") > 0;
        if (stats.evaluation.has_trustworthiness) {
            valid = valid && std::fprintf(file, "{\"value\": %.9f, \"k\": %u, \"cohort_size\": %llu}",
                stats.evaluation.sampled_trustworthiness, stats.evaluation.trustworthiness_k,
                static_cast<unsigned long long>(stats.evaluation.trustworthiness_cohort_size)) > 0;
        } else {
            valid = valid && std::fprintf(file, "null") > 0;
        }
        if (stats.evaluation.has_classification) {
            valid = valid && std::fprintf(file,
                ",\n    \"classification\": {\"sample_count\": %llu, \"training_count\": %u, "
                "\"accuracy_1nn\": %.9f, \"accuracy_5nn\": %.9f, \"accuracy_10nn\": %.9f, "
                "\"accuracy_20nn\": %.9f, \"accuracy_30nn\": %.9f, \"accuracy_40nn\": %.9f, "
                "\"accuracy_50nn\": %.9f}",
                static_cast<unsigned long long>(stats.evaluation.classification_sample_count),
                stats.evaluation.classification_training_count,
                stats.evaluation.accuracy_1nn, stats.evaluation.accuracy_5nn,
                stats.evaluation.accuracy_10nn, stats.evaluation.accuracy_20nn,
                stats.evaluation.accuracy_30nn, stats.evaluation.accuracy_40nn,
                stats.evaluation.accuracy_50nn) > 0;
        } else {
            valid = valid && std::fprintf(file, ",\n    \"classification\": null") > 0;
        }
        valid = valid && std::fprintf(file, "\n  }") > 0;
    }
    valid = valid && std::fprintf(file, "\n}\n") > 0 && std::fclose(file) == 0;
    if (!valid || std::rename(temporary_path.c_str(), path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        *error = "Cannot write stats output completely: " + path;
        return false;
    }
    return true;
}
