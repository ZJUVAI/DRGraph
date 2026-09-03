#include "graph.h"
#include "progress.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace {

const std::uint64_t kBinaryGraphRecordBytes = 12;

static_assert(sizeof(Edge) == kBinaryGraphRecordBytes,
              "Binary graph edge records must be 12 bytes");

bool StoreMergedWeight(double value, float* destination, std::string* error) {
    if (!std::isfinite(value) || value > std::numeric_limits<float>::max()) {
        *error = "Merged graph edge weight exceeds float range";
        return false;
    }
    *destination = static_cast<float>(value);
    return true;
}

std::uint64_t PackDirectedEdge(std::uint32_t target, float weight) {
    std::uint32_t weight_bits = 0;
    std::memcpy(&weight_bits, &weight, sizeof(weight_bits));
    return (static_cast<std::uint64_t>(target) << 32) | weight_bits;
}

float PackedWeight(std::uint64_t value) {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    float weight = 0.0f;
    std::memcpy(&weight, &bits, sizeof(weight));
    return weight;
}

bool FinalizeRowSortCsr(std::uint64_t vertex_count,
                        std::vector<std::uint64_t>* row_offsets,
                        std::vector<std::uint64_t>* packed,
                        CsrGraph* graph,
                        std::string* error,
                        std::uint64_t progress_offset) {
    CsrGraph built;
    built.vertex_count = vertex_count;
    built.row_offsets = std::move(*row_offsets);
    built.dst.resize(packed->size());
    built.weight.resize(packed->size());
    std::size_t output = 0;
    for (std::size_t vertex = 0; vertex < static_cast<std::size_t>(vertex_count); ++vertex) {
        const std::size_t begin = static_cast<std::size_t>(built.row_offsets[vertex]);
        const std::size_t end = static_cast<std::size_t>(built.row_offsets[vertex + 1]);
        std::sort(packed->begin() + begin, packed->begin() + end);
        built.row_offsets[vertex] = output;
        bool have_target = false;
        std::uint32_t target = 0;
        double sum = 0.0;
        for (std::size_t edge = begin; edge < end; ++edge) {
            const std::uint32_t next_target = static_cast<std::uint32_t>((*packed)[edge] >> 32);
            const float next_weight = PackedWeight((*packed)[edge]);
            if (!have_target || next_target != target) {
                if (have_target && !StoreMergedWeight(sum, &built.weight[output - 1], error)) return false;
                target = next_target;
                sum = next_weight;
                built.dst[output] = target;
                ++output;
                have_target = true;
            } else {
                sum += next_weight;
            }
        }
        if (have_target && !StoreMergedWeight(sum, &built.weight[output - 1], error)) return false;
        if ((vertex + 1) % 4096 == 0 || vertex + 1 == static_cast<std::size_t>(vertex_count)) {
            ProgressBar::Update(progress_offset + vertex + 1);
        }
    }
    built.row_offsets.back() = output;
    built.dst.resize(output);
    built.weight.resize(output);
    std::vector<std::uint64_t>().swap(*packed);
    *graph = std::move(built);
    return true;
}

bool BuildRowSortCsr(std::uint64_t vertex_count,
                     std::vector<Edge>* edges,
                     CsrGraph* graph,
                     std::string* error) {
    const std::uint64_t edge_count = edges->size();
    const std::uint64_t progress_total = edge_count * 2 + vertex_count;
    ProgressBar::Begin("Build row-sort CSR", progress_total);
    std::vector<std::uint64_t> row_offsets(static_cast<std::size_t>(vertex_count + 1), 0);
    for (std::size_t index = 0; index < edges->size(); ++index) {
        ++row_offsets[(*edges)[index].source + 1];
        ++row_offsets[(*edges)[index].target + 1];
        if ((index + 1) % 16384 == 0 || index + 1 == edges->size()) ProgressBar::Update(index + 1);
    }
    for (std::size_t vertex = 1; vertex < row_offsets.size(); ++vertex) {
        row_offsets[vertex] += row_offsets[vertex - 1];
    }
    std::vector<std::uint64_t> packed(static_cast<std::size_t>(row_offsets.back()));
    for (std::size_t index = edges->size(); index > 0; --index) {
        const Edge& edge = (*edges)[index - 1];
        packed[static_cast<std::size_t>(--row_offsets[edge.source + 1])] = PackDirectedEdge(edge.target, edge.weight);
        packed[static_cast<std::size_t>(--row_offsets[edge.target + 1])] = PackDirectedEdge(edge.source, edge.weight);
        const std::size_t completed = edges->size() - index + 1;
        if (completed % 16384 == 0 || completed == edges->size()) ProgressBar::Update(edge_count + completed);
    }
    for (std::size_t vertex = 0; vertex < static_cast<std::size_t>(vertex_count); ++vertex) {
        row_offsets[vertex] = row_offsets[vertex + 1];
    }
    row_offsets.front() = 0;
    row_offsets.back() = packed.size();
    std::vector<Edge>().swap(*edges);
    const bool success = FinalizeRowSortCsr(vertex_count, &row_offsets, &packed, graph, error, edge_count * 2);
    if (success) ProgressBar::Finish();
    else ProgressBar::Abort();
    return success;
}

bool ValidateAndNormalizeEdge(std::uint64_t vertex_count,
                              Edge* edge,
                              std::string* error) {
    if (edge->source >= vertex_count || edge->target >= vertex_count) {
        *error = "Graph contains an out-of-range vertex ID";
        return false;
    }
    if (edge->source == edge->target) {
        *error = "Self-loops are not allowed in the graph";
        return false;
    }
    if (!std::isfinite(edge->weight) || edge->weight <= 0.0f) {
        *error = "Graph contains an invalid edge weight";
        return false;
    }
    if (edge->source > edge->target) std::swap(edge->source, edge->target);
    return true;
}

bool ValidateAndNormalizeEdges(std::uint64_t vertex_count,
                               std::vector<Edge>* edges,
                               std::string* error) {
    ProgressBar::Begin("Validate graph edges", edges->size());
    for (std::size_t index = 0; index < edges->size(); ++index) {
        if (!ValidateAndNormalizeEdge(vertex_count, &(*edges)[index], error)) {
            ProgressBar::Abort();
            return false;
        }
        if ((index + 1) % 16384 == 0 || index + 1 == edges->size()) ProgressBar::Update(index + 1);
    }
    ProgressBar::Finish();
    return true;
}

}  // namespace

bool ValidateCsrGraph(const CsrGraph& graph,
                      std::string* error,
                      CsrWeightPolicy weight_policy) {
    if (error == nullptr) return false;
    if (graph.vertex_count == 0 ||
        graph.vertex_count > std::numeric_limits<std::uint32_t>::max() ||
        graph.vertex_count > std::numeric_limits<std::size_t>::max() - 1) {
        *error = "Probability CSR vertex count is invalid";
        return false;
    }
    const std::size_t vertices = static_cast<std::size_t>(graph.vertex_count);
    if (graph.row_offsets.size() != vertices + 1 ||
        graph.dst.size() != graph.weight.size() ||
        graph.row_offsets.front() != 0 ||
        graph.row_offsets.back() != graph.dst.size()) {
        *error = "Probability CSR array sizes are inconsistent";
        return false;
    }
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
        if (graph.row_offsets[vertex] > graph.row_offsets[vertex + 1] ||
            graph.row_offsets[vertex + 1] > graph.dst.size()) {
            *error = "Probability CSR row offsets are invalid";
            return false;
        }
    }
    for (std::size_t edge = 0; edge < graph.dst.size(); ++edge) {
        const bool invalid_weight = !std::isfinite(graph.weight[edge]) ||
            (weight_policy == CsrWeightPolicy::RequirePositive
                ? graph.weight[edge] <= 0.0f
                : graph.weight[edge] < 0.0f);
        if (graph.dst[edge] >= graph.vertex_count || invalid_weight) {
            *error = "Probability CSR contains an invalid vertex or weight";
            return false;
        }
    }
    return true;
}

bool BuildUndirectedCsr(std::uint64_t vertex_count,
                        std::vector<Edge>* edges,
                        CsrGraph* graph,
                        std::string* error) {
    if (edges == nullptr || graph == nullptr || error == nullptr) return false;
    if (vertex_count == 0 || vertex_count > std::numeric_limits<std::uint32_t>::max() ||
        vertex_count > std::numeric_limits<std::size_t>::max() - 1) {
        *error = "Graph N must be positive and fit in uint32_t vertex IDs";
        return false;
    }
    if (edges->size() > std::numeric_limits<std::size_t>::max() / 2) {
        *error = "Graph has too many edges to build a bidirectional CSR";
        return false;
    }
    if (!ValidateAndNormalizeEdges(vertex_count, edges, error)) return false;
    return BuildRowSortCsr(vertex_count, edges, graph, error);
}
