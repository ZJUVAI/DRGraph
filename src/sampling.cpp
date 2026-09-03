#include "sampling.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

bool SetSamplingError(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

bool CheckGraphForSampling(const CsrGraph& graph, std::string* error) {
    if (!ValidateCsrGraph(graph, error, CsrWeightPolicy::AllowZero)) return false;
    if (!graph.negative_sampling_weight.empty() &&
        graph.negative_sampling_weight.size() != static_cast<std::size_t>(graph.vertex_count)) {
        return SetSamplingError(error, "Negative-sampling weight array size is inconsistent");
    }
    return true;
}

bool BuildRowAlias(const std::vector<float>& weights,
                   std::size_t begin,
                   std::size_t end,
                   AliasTable* table,
                   std::string* error) {
    if (table == nullptr || error == nullptr || begin > end || end > weights.size() ||
        end > table->probability.size() || end > table->alias.size() || begin == end ||
        end - begin > std::numeric_limits<std::uint32_t>::max()) {
        return SetSamplingError(error, "Alias-table row range is invalid");
    }
    const std::size_t count = end - begin;
    double total = 0.0;
    for (std::size_t index = begin; index < end; ++index) total += weights[index];
    if (!std::isfinite(total) || total <= 0.0) {
        *error = "Positive-edge alias table contains a non-positive row weight";
        return false;
    }

    std::vector<double> scaled(count);
    std::vector<std::size_t> small;
    std::vector<std::size_t> large;
    small.reserve(count);
    large.reserve(count);
    for (std::size_t local = 0; local < count; ++local) {
        table->alias[begin + local] = static_cast<std::uint32_t>(begin + local);
        scaled[local] = static_cast<double>(weights[begin + local]) * count / total;
        if (scaled[local] < 1.0) small.push_back(local);
        else large.push_back(local);
    }
    while (!small.empty() && !large.empty()) {
        const std::size_t low = small.back();
        small.pop_back();
        const std::size_t high = large.back();
        large.pop_back();
        table->probability[begin + low] = static_cast<float>(scaled[low]);
        table->alias[begin + low] = static_cast<std::uint32_t>(begin + high);
        scaled[high] += scaled[low] - 1.0;
        if (scaled[high] < 1.0) small.push_back(high);
        else large.push_back(high);
    }
    while (!small.empty()) {
        table->probability[begin + small.back()] = 1.0f;
        small.pop_back();
    }
    while (!large.empty()) {
        table->probability[begin + large.back()] = 1.0f;
        large.pop_back();
    }
    return true;
}

}  // namespace

std::size_t AliasTable::size() const {
    return probability.size();
}

bool BuildAliasTable(const std::vector<float>& weights, AliasTable* table, std::string* error) {
    if (table == nullptr || error == nullptr || weights.empty() ||
        weights.size() > std::numeric_limits<std::uint32_t>::max()) {
        if (error != nullptr) *error = "Alias-table weight count must be non-empty and fit in uint32_t";
        return false;
    }
    double total = 0.0;
    for (std::size_t index = 0; index < weights.size(); ++index) {
        if (!std::isfinite(weights[index]) || weights[index] < 0.0f) {
            *error = "Alias table contains an invalid weight";
            return false;
        }
        total += weights[index];
    }
    if (!std::isfinite(total) || total <= 0.0) {
            *error = "Alias-table weights must have a positive finite sum";
        return false;
    }

    AliasTable built;
    built.probability.assign(weights.size(), 0.0f);
    built.alias.resize(weights.size());
    if (!BuildRowAlias(weights, 0, weights.size(), &built, error)) return false;
    *table = std::move(built);
    return true;
}

static bool BuildNegativeAliasTableUnchecked(const CsrGraph& graph,
                                             AliasTable* table,
                                             std::string* error) {
    if (table == nullptr || error == nullptr) return false;
    if (!graph.negative_sampling_weight.empty()) {
        return BuildAliasTable(graph.negative_sampling_weight, table, error);
    }
    std::vector<float> weights(static_cast<std::size_t>(graph.vertex_count), 0.0f);
    for (std::size_t row = 0; row < weights.size(); ++row) {
        double degree = 0.0;
        for (std::size_t edge = static_cast<std::size_t>(graph.row_offsets[row]);
             edge < static_cast<std::size_t>(graph.row_offsets[row + 1]); ++edge) {
            degree += graph.weight[edge];
        }
        weights[row] = degree > 0.0 ? static_cast<float>(std::pow(degree, 0.75)) : 0.0f;
    }
    bool any_weight = false;
    for (std::size_t row = 0; row < weights.size(); ++row) any_weight = any_weight || weights[row] > 0.0f;
    if (!any_weight) std::fill(weights.begin(), weights.end(), 1.0f);
    return BuildAliasTable(weights, table, error);
}

static bool BuildPositiveAliasTablesUnchecked(const CsrGraph& graph,
                                              AliasTable* source_table,
                                              AliasTable* edge_table,
                                              std::string* error) {
    if (source_table == nullptr || edge_table == nullptr || error == nullptr ||
        graph.weight.empty() ||
        graph.weight.size() > std::numeric_limits<std::uint32_t>::max()) {
        if (error != nullptr && error->empty()) *error = "Positive-edge alias-table input is invalid";
        return false;
    }
    bool uniform_edge_weight = graph.uniform_edge_weights;
    const float first_weight = graph.weight.front();
    if (!uniform_edge_weight) {
        uniform_edge_weight = std::isfinite(first_weight) && first_weight > 0.0f;
        for (std::size_t edge = 1; edge < graph.weight.size() && uniform_edge_weight; ++edge) {
            uniform_edge_weight = graph.weight[edge] == first_weight;
        }
    }

    std::vector<float> source_weights(static_cast<std::size_t>(graph.vertex_count), 0.0f);
    if (uniform_edge_weight) {
        for (std::size_t source = 0; source < source_weights.size(); ++source) {
            const std::size_t degree = static_cast<std::size_t>(graph.row_offsets[source + 1] -
                                                                  graph.row_offsets[source]);
            source_weights[source] = static_cast<float>(degree) * first_weight;
        }
        AliasTable built_sources;
        if (!BuildAliasTable(source_weights, &built_sources, error)) return false;
        *source_table = std::move(built_sources);
        edge_table->probability.clear();
        edge_table->alias.clear();
        return true;
    }

    AliasTable built_edges;
    built_edges.probability.assign(graph.weight.size(), 0.0f);
    built_edges.alias.resize(graph.weight.size());
    for (std::size_t source = 0; source < source_weights.size(); ++source) {
        const std::size_t begin = static_cast<std::size_t>(graph.row_offsets[source]);
        const std::size_t end = static_cast<std::size_t>(graph.row_offsets[source + 1]);
        if (begin == end) continue;
        double total = 0.0;
        for (std::size_t edge = begin; edge < end; ++edge) total += graph.weight[edge];
        if (!std::isfinite(total) || total <= 0.0) {
            *error = "Positive-edge alias table contains a non-positive row weight";
            return false;
        }
        source_weights[source] = static_cast<float>(total);
        if (!BuildRowAlias(graph.weight, begin, end, &built_edges, error)) return false;
    }
    AliasTable built_sources;
    if (!BuildAliasTable(source_weights, &built_sources, error)) return false;
    *source_table = std::move(built_sources);
    *edge_table = std::move(built_edges);
    return true;
}

bool BuildNegativeAliasTable(const CsrGraph& graph, AliasTable* table, std::string* error) {
    if (table == nullptr || error == nullptr || !CheckGraphForSampling(graph, error)) return false;
    return BuildNegativeAliasTableUnchecked(graph, table, error);
}

bool BuildPositiveAliasTables(const CsrGraph& graph,
                              AliasTable* source_table,
                              AliasTable* edge_table,
                              std::string* error) {
    if (source_table == nullptr || edge_table == nullptr || error == nullptr ||
        !CheckGraphForSampling(graph, error)) return false;
    return BuildPositiveAliasTablesUnchecked(graph, source_table, edge_table, error);
}

bool BuildLayoutAliasTables(const CsrGraph& graph,
                            AliasTable* source_table,
                            AliasTable* edge_table,
                            AliasTable* negative_table,
                            std::string* error) {
    if (source_table == nullptr || edge_table == nullptr || negative_table == nullptr ||
        error == nullptr || !CheckGraphForSampling(graph, error)) return false;
    return BuildPositiveAliasTablesUnchecked(graph, source_table, edge_table, error) &&
        BuildNegativeAliasTableUnchecked(graph, negative_table, error);
}
