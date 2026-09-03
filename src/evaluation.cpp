#include "evaluation.h"


#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <vector>

namespace {

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

bool ValidateEmbedding(const CsrGraph& graph, const Embedding& embedding, std::string* error) {
    if (embedding.vertex_count != graph.vertex_count || embedding.dimension == 0) {
        SetError(error, "Embedding vertex count must match the graph and dimension must be positive");
        return false;
    }
    if (embedding.vertex_count > std::numeric_limits<std::size_t>::max() / embedding.dimension ||
        embedding.values.size() != static_cast<std::size_t>(embedding.vertex_count * embedding.dimension)) {
        SetError(error, "Embedding coordinate array length is invalid");
        return false;
    }
    for (std::size_t index = 0; index < embedding.values.size(); ++index) {
        if (!std::isfinite(embedding.values[index])) {
            SetError(error, "Embedding coordinates must be finite");
            return false;
        }
    }
    return true;
}

bool SquaredDistance(const Embedding& embedding,
                     std::uint32_t left,
                     std::uint32_t right,
                     double* squared_distance) {
    const std::size_t dimension = embedding.dimension;
    const std::size_t left_offset = static_cast<std::size_t>(left) * dimension;
    const std::size_t right_offset = static_cast<std::size_t>(right) * dimension;
    double sum = 0.0;
    for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) {
        const double difference = static_cast<double>(embedding.values[left_offset + coordinate]) -
                                  static_cast<double>(embedding.values[right_offset + coordinate]);
        sum += difference * difference;
        if (!std::isfinite(sum)) return false;
    }
    *squared_distance = sum;
    return true;
}

struct NeighborCandidate {
    double squared_distance;
    std::uint32_t vertex;
};

bool CandidateLess(const NeighborCandidate& left, const NeighborCandidate& right) {
    if (left.squared_distance != right.squared_distance) {
        return left.squared_distance < right.squared_distance;
    }
    return left.vertex < right.vertex;
}

void KeepNearest(const NeighborCandidate& candidate,
                 std::size_t limit,
                 std::vector<NeighborCandidate>* nearest) {
    if (limit == 0) return;
    if (nearest->size() < limit) {
        nearest->push_back(candidate);
        std::push_heap(nearest->begin(), nearest->end(), CandidateLess);
    } else if (CandidateLess(candidate, nearest->front())) {
        std::pop_heap(nearest->begin(), nearest->end(), CandidateLess);
        nearest->back() = candidate;
        std::push_heap(nearest->begin(), nearest->end(), CandidateLess);
    }
}

struct KdNode {
    std::uint32_t vertex;
    std::int32_t left;
    std::int32_t right;
    std::uint32_t axis;
};

class ExactLowDimensionalKnn {
public:
    explicit ExactLowDimensionalKnn(const Embedding& embedding) : embedding_(embedding) {
        order_.resize(static_cast<std::size_t>(embedding.vertex_count));
        for (std::uint32_t vertex = 0; vertex < embedding.vertex_count; ++vertex) order_[vertex] = vertex;
    }

    bool Build(std::string* error) {
        if (embedding_.dimension == 0 || embedding_.dimension > 3) {
            SetError(error, "Exact low-dimensional neighborhood evaluation supports dimensions 1, 2, and 3");
            return false;
        }
        nodes_.reserve(order_.size());
        root_ = BuildRange(0, order_.size());
        return true;
    }

    void Search(std::uint32_t source, std::uint32_t neighbor_count,
                std::vector<NeighborCandidate>* nearest) const {
        nearest->clear();
        nearest->reserve(neighbor_count);
        SearchNode(root_, source, neighbor_count, nearest);
    }

private:
    double Coordinate(std::uint32_t vertex, std::uint32_t axis) const {
        return embedding_.values[static_cast<std::size_t>(vertex) * embedding_.dimension + axis];
    }

    std::uint32_t ChooseAxis(std::size_t begin, std::size_t end) const {
        std::uint32_t best_axis = 0;
        double best_span = -1.0;
        for (std::uint32_t axis = 0; axis < embedding_.dimension; ++axis) {
            double low = Coordinate(order_[begin], axis);
            double high = low;
            for (std::size_t position = begin + 1; position < end; ++position) {
                const double value = Coordinate(order_[position], axis);
                low = std::min(low, value);
                high = std::max(high, value);
            }
            const double span = high - low;
            if (span > best_span) {
                best_span = span;
                best_axis = axis;
            }
        }
        return best_axis;
    }

    std::int32_t BuildRange(std::size_t begin, std::size_t end) {
        if (begin == end) return -1;
        const std::uint32_t axis = ChooseAxis(begin, end);
        const std::size_t middle = begin + (end - begin) / 2;
        std::nth_element(order_.begin() + begin, order_.begin() + middle, order_.begin() + end,
            [this, axis](std::uint32_t left, std::uint32_t right) {
                const double left_value = Coordinate(left, axis);
                const double right_value = Coordinate(right, axis);
                return left_value == right_value ? left < right : left_value < right_value;
            });
        const std::int32_t node_index = static_cast<std::int32_t>(nodes_.size());
        nodes_.push_back(KdNode{order_[middle], -1, -1, axis});
        nodes_[node_index].left = BuildRange(begin, middle);
        nodes_[node_index].right = BuildRange(middle + 1, end);
        return node_index;
    }

    void Consider(std::uint32_t source, std::uint32_t candidate, std::uint32_t neighbor_count,
                  std::vector<NeighborCandidate>* nearest) const {
        if (candidate == source) return;
        double squared_distance = 0.0;
        SquaredDistance(embedding_, source, candidate, &squared_distance);
        const NeighborCandidate value = {squared_distance, candidate};
        KeepNearest(value, neighbor_count, nearest);
    }

    void SearchNode(std::int32_t node_index, std::uint32_t source, std::uint32_t neighbor_count,
                    std::vector<NeighborCandidate>* nearest) const {
        if (node_index < 0) return;
        const KdNode& node = nodes_[static_cast<std::size_t>(node_index)];
        const double split = Coordinate(node.vertex, node.axis);
        const double source_coordinate = Coordinate(source, node.axis);
        const std::int32_t near_child = source_coordinate < split ? node.left : node.right;
        const std::int32_t far_child = source_coordinate < split ? node.right : node.left;
        SearchNode(near_child, source, neighbor_count, nearest);
        Consider(source, node.vertex, neighbor_count, nearest);
        const double axis_distance = source_coordinate - split;
        const double cutoff = nearest->size() < neighbor_count
                                  ? std::numeric_limits<double>::infinity()
                                  : nearest->front().squared_distance;
        if (axis_distance * axis_distance <= cutoff) SearchNode(far_child, source, neighbor_count, nearest);
    }

    const Embedding& embedding_;
    std::vector<std::uint32_t> order_;
    std::vector<KdNode> nodes_;
    std::int32_t root_ = -1;
};

bool SquaredHighDistance(const EvaluationCohort& cohort,
                         std::uint64_t left,
                         std::uint64_t right,
                         double* squared_distance) {
    const std::size_t dimension = static_cast<std::size_t>(cohort.dimension);
    const std::size_t left_offset = static_cast<std::size_t>(left) * dimension;
    const std::size_t right_offset = static_cast<std::size_t>(right) * dimension;
    double sum = 0.0;
    for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) {
        const double difference = static_cast<double>(cohort.values[left_offset + coordinate]) -
                                  static_cast<double>(cohort.values[right_offset + coordinate]);
        sum += difference * difference;
        if (!std::isfinite(sum)) return false;
    }
    *squared_distance = sum;
    return true;
}

bool SquaredCohortEmbeddingDistance(const EvaluationCohort& cohort,
                                    const Embedding& embedding,
                                    std::uint64_t left,
                                    std::uint64_t right,
                                    double* squared_distance) {
    return SquaredDistance(embedding, cohort.vertex_ids[static_cast<std::size_t>(left)],
                           cohort.vertex_ids[static_cast<std::size_t>(right)], squared_distance);
}

bool FindNearestByBruteForce(const Embedding& embedding,
                             std::uint32_t source,
                             std::uint32_t neighbor_count,
                             std::vector<NeighborCandidate>* nearest,
                             std::string* error) {
    nearest->clear();
    nearest->reserve(neighbor_count);
    for (std::uint32_t candidate = 0; candidate < embedding.vertex_count; ++candidate) {
        if (candidate == source) continue;
        double squared_distance = 0.0;
        if (!SquaredDistance(embedding, source, candidate, &squared_distance)) {
            SetError(error, "Embedded distance calculation overflowed");
            return false;
        }
        const NeighborCandidate value = {squared_distance, candidate};
        KeepNearest(value, neighbor_count, nearest);
    }
    std::sort(nearest->begin(), nearest->end(), CandidateLess);
    return true;
}

std::uint64_t EffectiveSampleCount(std::uint64_t requested, std::uint64_t vertex_count) {
    return requested == 0 || requested >= vertex_count ? vertex_count : requested;
}

std::uint32_t SampleVertex(std::uint64_t sample,
                           std::uint64_t sample_count,
                           std::uint64_t vertex_count) {
    return static_cast<std::uint32_t>((sample * vertex_count) / sample_count);
}

void BuildNeighborhood(const CsrGraph& graph,
                       std::uint32_t source,
                       std::uint32_t hop_limit,
                       std::vector<std::uint32_t>* neighborhood) {
    neighborhood->clear();
    if (hop_limit == 0) return;
    std::vector<std::uint32_t> distance(static_cast<std::size_t>(graph.vertex_count),
                                        std::numeric_limits<std::uint32_t>::max());
    std::queue<std::uint32_t> queue;
    distance[source] = 0;
    queue.push(source);
    while (!queue.empty()) {
        const std::uint32_t current = queue.front();
        queue.pop();
        if (distance[current] == hop_limit) continue;
        for (std::uint64_t edge = graph.row_offsets[current]; edge < graph.row_offsets[current + 1]; ++edge) {
            const std::uint32_t target = graph.dst[static_cast<std::size_t>(edge)];
            if (distance[target] != std::numeric_limits<std::uint32_t>::max()) continue;
            distance[target] = distance[current] + 1;
            if (target != source) neighborhood->push_back(target);
            queue.push(target);
        }
    }
    std::sort(neighborhood->begin(), neighborhood->end());
}

}  // namespace

bool EvaluateLayout(const CsrGraph& graph,
                           const Embedding& embedding,
                           const EvaluationConfig& config,
                           EvaluationResult* result,
                           std::string* error) {
    if (result == nullptr) {
        SetError(error, "Evaluation result output is null");
        return false;
    }
    if (!ValidateCsrGraph(graph, error, CsrWeightPolicy::RequirePositive) ||
        !ValidateEmbedding(graph, embedding, error)) return false;

    double distance_sum = 0.0;
    double squared_distance_sum = 0.0;
    double edge_count = 0.0;
    for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
        const std::uint64_t begin = graph.row_offsets[static_cast<std::size_t>(source)];
        const std::uint64_t end = graph.row_offsets[static_cast<std::size_t>(source + 1)];
        for (std::uint64_t edge = begin; edge < end; ++edge) {
            const std::size_t edge_index = static_cast<std::size_t>(edge);
            double squared_distance = 0.0;
            if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source), graph.dst[edge_index],
                                 &squared_distance)) {
                SetError(error, "Embedded distance calculation overflowed");
                return false;
            }
            const double distance = std::sqrt(squared_distance);
            distance_sum += distance;
            squared_distance_sum += squared_distance;
            edge_count += 1.0;
            if (!std::isfinite(distance_sum) || !std::isfinite(squared_distance_sum)) {
                SetError(error, "Evaluation accumulator overflowed");
                return false;
            }
        }
    }

    const std::uint64_t sample_count = EffectiveSampleCount(config.evaluation_sample_vertices,
                                                             graph.vertex_count);
    const std::uint64_t global_sample_count = config.evaluation_sample_vertices == 0
        ? std::min<std::uint64_t>(graph.vertex_count,
                                  std::max<std::uint64_t>(1, 10000000000ULL /
                                                             (graph.vertex_count + graph.dst.size())))
        : sample_count;
    const std::uint64_t available_neighbors = graph.vertex_count - 1;
    double jaccard_sum = 0.0;
    std::uint64_t evaluated_vertices = 0;
    std::vector<NeighborCandidate> nearest;
    std::vector<std::uint32_t> graph_neighborhood;
    std::vector<bool> graph_neighbor_flags(static_cast<std::size_t>(graph.vertex_count), false);
    ExactLowDimensionalKnn embedding_index(embedding);
    const bool use_kd_tree = embedding.dimension <= 3;
    if (use_kd_tree && !embedding_index.Build(error)) return false;

    for (std::uint64_t sample = 0; sample < sample_count; ++sample) {
        const std::uint32_t source = SampleVertex(sample, sample_count, graph.vertex_count);
        BuildNeighborhood(graph, source, config.neighborhood_hop_limit, &graph_neighborhood);
        const std::uint64_t neighbor_count = std::min<std::uint64_t>(graph_neighborhood.size(), available_neighbors);
        if (neighbor_count == 0) continue;
        std::fill(graph_neighbor_flags.begin(), graph_neighbor_flags.end(), false);
        for (std::size_t index = 0; index < neighbor_count; ++index) {
            graph_neighbor_flags[graph_neighborhood[index]] = true;
        }
        if (use_kd_tree) {
            embedding_index.Search(source, static_cast<std::uint32_t>(neighbor_count), &nearest);
        } else if (!FindNearestByBruteForce(embedding, source, static_cast<std::uint32_t>(neighbor_count),
                                            &nearest, error)) {
            return false;
        }
        std::uint64_t hits = 0;
        for (std::size_t index = 0; index < nearest.size(); ++index) {
            if (graph_neighbor_flags[nearest[index].vertex]) ++hits;
        }
        const double union_size = static_cast<double>(neighbor_count * 2 - hits);
        jaccard_sum += union_size == 0.0 ? 0.0 : static_cast<double>(hits) / union_size;
        ++evaluated_vertices;
    }

    EvaluationResult evaluated;
    double neighbor_distance_sum = 0.0;
    double neighbor_squared_distance_sum = 0.0;
    std::uint64_t undirected_edge_count = 0;
    for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
        for (std::uint64_t edge = graph.row_offsets[static_cast<std::size_t>(source)];
             edge < graph.row_offsets[static_cast<std::size_t>(source + 1)]; ++edge) {
            const std::size_t edge_index = static_cast<std::size_t>(edge);
            if (source >= graph.dst[edge_index]) continue;
            double squared_distance = 0.0;
            if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source), graph.dst[edge_index],
                                 &squared_distance)) {
                SetError(error, "Embedded distance calculation overflowed");
                return false;
            }
            const double distance = std::sqrt(squared_distance);
            neighbor_distance_sum += distance;
            neighbor_squared_distance_sum += squared_distance;
            ++undirected_edge_count;
        }
    }
    evaluated.undirected_edge_count = undirected_edge_count;
    if (!graph.dst.empty()) {
        if (squared_distance_sum == 0.0) {
            evaluated.average_directed_neighbor_distance = 0.0;
            evaluated.normalized_edge_stress = 1.0;
            evaluated.has_edge_metrics = true;
        } else {
            const double scale = distance_sum / squared_distance_sum;
            double residual_sum = 0.0;
            for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
                for (std::uint64_t edge = graph.row_offsets[static_cast<std::size_t>(source)];
                     edge < graph.row_offsets[static_cast<std::size_t>(source + 1)]; ++edge) {
                    double squared_distance = 0.0;
                    if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source),
                                         graph.dst[static_cast<std::size_t>(edge)], &squared_distance)) {
                        SetError(error, "Embedded distance calculation overflowed");
                        return false;
                    }
                    const double residual = 1.0 - scale * std::sqrt(squared_distance);
                    residual_sum += residual * residual;
                }
            }
            evaluated.average_directed_neighbor_distance = distance_sum / edge_count;
            evaluated.normalized_edge_stress = std::sqrt(residual_sum / edge_count);
            evaluated.has_edge_metrics = true;
        }
    }
    if (undirected_edge_count != 0) {
        if (neighbor_squared_distance_sum == 0.0) {
            evaluated.stress_neighbors = 1.0;
        } else {
            const double scale = neighbor_distance_sum / neighbor_squared_distance_sum;
            double residual_sum = 0.0;
            for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
                for (std::uint64_t edge = graph.row_offsets[static_cast<std::size_t>(source)];
                     edge < graph.row_offsets[static_cast<std::size_t>(source + 1)]; ++edge) {
                    const std::size_t edge_index = static_cast<std::size_t>(edge);
                    if (source >= graph.dst[edge_index]) continue;
                    double squared_distance = 0.0;
                    if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source), graph.dst[edge_index],
                                         &squared_distance)) {
                        SetError(error, "Embedded distance calculation overflowed");
                        return false;
                    }
                    const double residual = scale * std::sqrt(squared_distance) - 1.0;
                    residual_sum += residual * residual;
                }
            }
            evaluated.stress_neighbors = residual_sum / static_cast<double>(undirected_edge_count);
        }
        evaluated.has_stress_neighbors = true;
    }

    std::vector<std::uint32_t> distances(static_cast<std::size_t>(graph.vertex_count));
    std::queue<std::uint32_t> queue;
    double ratio_sum = 0.0;
    double ratio_squared_sum = 0.0;
    std::uint64_t pair_count = 0;
    for (std::uint64_t sample = 0; sample < global_sample_count; ++sample) {
        const std::uint32_t source = SampleVertex(sample, global_sample_count, graph.vertex_count);
        std::fill(distances.begin(), distances.end(), std::numeric_limits<std::uint32_t>::max());
        while (!queue.empty()) queue.pop();
        distances[source] = 0;
        queue.push(source);
        while (!queue.empty()) {
            const std::uint32_t current = queue.front();
            queue.pop();
            const std::uint64_t begin = graph.row_offsets[current];
            const std::uint64_t end = graph.row_offsets[current + 1];
            for (std::uint64_t edge = begin; edge < end; ++edge) {
                const std::uint32_t target = graph.dst[static_cast<std::size_t>(edge)];
                if (distances[target] == std::numeric_limits<std::uint32_t>::max()) {
                    distances[target] = distances[current] + 1;
                    queue.push(target);
                }
            }
        }
        for (std::uint32_t target = 0; target < graph.vertex_count; ++target) {
            if (target == source || distances[target] == std::numeric_limits<std::uint32_t>::max()) continue;
            double squared_distance = 0.0;
            if (!SquaredDistance(embedding, source, target, &squared_distance)) {
                SetError(error, "Embedded distance calculation overflowed");
                return false;
            }
            const double ratio = std::sqrt(squared_distance) / distances[target];
            ratio_sum += ratio;
            ratio_squared_sum += ratio * ratio;
            ++pair_count;
        }
    }
    evaluated.global_stress_pair_count = pair_count;
    if (pair_count != 0) {
        if (ratio_squared_sum == 0.0) {
            evaluated.global_stress = 1.0;
        } else {
            // The residual expands to pair_count - 2*scale*ratio_sum +
            // scale^2*ratio_squared_sum, so a second BFS pass is unnecessary.
            const double residual_sum = std::max(
                0.0, static_cast<double>(pair_count) -
                (ratio_sum * ratio_sum) / ratio_squared_sum);
            evaluated.global_stress = residual_sum /
                (static_cast<double>(global_sample_count) * static_cast<double>(graph.vertex_count));
        }
        evaluated.has_global_stress = true;
    }

    double layout_proximity_sum = 0.0;
    double graph_weight_sum = 0.0;
    for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
        for (std::uint64_t edge = graph.row_offsets[source]; edge < graph.row_offsets[source + 1]; ++edge) {
            const std::size_t edge_index = static_cast<std::size_t>(edge);
            double squared_distance = 0.0;
            if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source),
                                 graph.dst[edge_index], &squared_distance)) {
                SetError(error, "Embedded distance calculation overflowed");
                return false;
            }
            layout_proximity_sum += 1.0 / (1.0 + squared_distance);
            graph_weight_sum += graph.weight[edge_index];
        }
    }
    if (!graph.dst.empty() && layout_proximity_sum > 0.0 && std::isfinite(layout_proximity_sum) &&
        graph_weight_sum > 0.0 && std::isfinite(graph_weight_sum)) {
        double loss = 0.0;
        for (std::uint64_t source = 0; source < graph.vertex_count; ++source) {
            for (std::uint64_t edge = graph.row_offsets[source]; edge < graph.row_offsets[source + 1]; ++edge) {
                const std::size_t edge_index = static_cast<std::size_t>(edge);
                double squared_distance = 0.0;
                if (!SquaredDistance(embedding, static_cast<std::uint32_t>(source), graph.dst[edge_index],
                                     &squared_distance)) {
                    SetError(error, "Embedded distance calculation overflowed");
                    return false;
                }
                const double ns = static_cast<double>(graph.weight[edge_index]) / graph_weight_sum;
                const double lp = (1.0 / (1.0 + squared_distance)) / layout_proximity_sum;
                loss += ns * std::log(ns / lp);
            }
        }
        evaluated.kl_divergence = loss;
        evaluated.has_kl_divergence = std::isfinite(loss);
        if (!evaluated.has_kl_divergence) {
            SetError(error, "KL divergence calculation overflowed");
            return false;
        }
    }
    if (evaluated_vertices != 0) {
        evaluated.neighborhood_jaccard = jaccard_sum / static_cast<double>(evaluated_vertices);
        evaluated.has_neighborhood_jaccard = true;
    }
    evaluated.evaluated_vertex_count = evaluated_vertices;
    evaluated.directed_edge_count = static_cast<std::uint64_t>(graph.dst.size());
    *result = evaluated;
    return true;
}

bool EvaluateCohortTrustworthiness(const EvaluationCohort& cohort,
                                   const Embedding& embedding,
                                   std::uint32_t requested_k,
                                   EvaluationResult* result,
                                   std::string* error) {
    if (result == nullptr) {
        SetError(error, "Evaluation result output is null");
        return false;
    }
    if (cohort.dimension == 0 || cohort.vertex_ids.size() > std::numeric_limits<std::size_t>::max() / cohort.dimension ||
        cohort.values.size() != cohort.vertex_ids.size() * static_cast<std::size_t>(cohort.dimension)) {
        SetError(error, "High-dimensional evaluation cohort array length is invalid");
        return false;
    }
    const std::uint64_t count = static_cast<std::uint64_t>(cohort.vertex_ids.size());
    for (std::size_t index = 0; index < cohort.vertex_ids.size(); ++index) {
        if (cohort.vertex_ids[index] >= embedding.vertex_count) {
            SetError(error, "High-dimensional evaluation cohort contains an out-of-range vertex");
            return false;
        }
    }
    if (embedding.dimension == 0 || embedding.values.size() !=
        static_cast<std::size_t>(embedding.vertex_count) * embedding.dimension) {
        SetError(error, "Embedding coordinate array length is invalid");
        return false;
    }
    if (count < 3 || requested_k == 0) {
        result->trustworthiness_cohort_size = count;
        result->trustworthiness_k = 0;
        result->has_trustworthiness = false;
        return true;
    }
    const std::uint32_t neighbor_count = std::min<std::uint32_t>(requested_k, static_cast<std::uint32_t>(count - 2));
    std::vector<NeighborCandidate> high_candidates;
    std::vector<NeighborCandidate> low_candidates;
    high_candidates.reserve(static_cast<std::size_t>(count - 1));
    low_candidates.reserve(static_cast<std::size_t>(count - 1));
    double rank_penalty_sum = 0.0;
    for (std::uint64_t source = 0; source < count; ++source) {
        high_candidates.clear();
        low_candidates.clear();
        for (std::uint64_t target = 0; target < count; ++target) {
            if (source == target) continue;
            double high_distance = 0.0;
            double low_distance = 0.0;
            if (!SquaredHighDistance(cohort, source, target, &high_distance) ||
                !SquaredCohortEmbeddingDistance(cohort, embedding, source, target, &low_distance)) {
                SetError(error, "Trustworthiness distance calculation overflowed");
                return false;
            }
            const std::uint32_t vertex = static_cast<std::uint32_t>(target);
            high_candidates.push_back(NeighborCandidate{high_distance, vertex});
            low_candidates.push_back(NeighborCandidate{low_distance, vertex});
        }
        std::sort(high_candidates.begin(), high_candidates.end(), CandidateLess);
        std::sort(low_candidates.begin(), low_candidates.end(), CandidateLess);
        std::vector<std::uint32_t> high_rank(static_cast<std::size_t>(count), 0);
        for (std::size_t rank = 0; rank < high_candidates.size(); ++rank) {
            high_rank[high_candidates[rank].vertex] = static_cast<std::uint32_t>(rank + 1);
        }
        for (std::uint32_t neighbor = 0; neighbor < neighbor_count; ++neighbor) {
            const std::uint32_t rank = high_rank[low_candidates[neighbor].vertex];
            if (rank > neighbor_count) rank_penalty_sum += rank - neighbor_count;
        }
    }
    const double denominator = static_cast<double>(count) * neighbor_count *
        (2.0 * count - 3.0 * neighbor_count - 1.0);
    if (denominator <= 0.0) {
        SetError(error, "Trustworthiness denominator is invalid");
        return false;
    }
    const double trustworthiness = 1.0 - 2.0 * rank_penalty_sum / denominator;
    if (!std::isfinite(trustworthiness) || trustworthiness < -1e-12 || trustworthiness > 1.0 + 1e-12) {
        SetError(error, "Trustworthiness result is out of range");
        return false;
    }
    result->sampled_trustworthiness = std::max(0.0, std::min(1.0, trustworthiness));
    result->trustworthiness_cohort_size = count;
    result->trustworthiness_k = neighbor_count;
    result->has_trustworthiness = true;
    return true;
}

bool EvaluateClassification(const Embedding& embedding,
                            const std::vector<std::uint64_t>& labels,
                            std::uint64_t requested_sample_vertices,
                            EvaluationResult* result,
                            std::string* error) {
    if (result == nullptr) {
        SetError(error, "Evaluation result output is null");
        return false;
    }
    if (embedding.vertex_count == 0 || embedding.dimension == 0 ||
        embedding.vertex_count > std::numeric_limits<std::size_t>::max() / embedding.dimension ||
        embedding.values.size() != static_cast<std::size_t>(embedding.vertex_count * embedding.dimension)) {
        SetError(error, "Embedding shape is invalid");
        return false;
    }
    if (labels.size() != static_cast<std::size_t>(embedding.vertex_count)) {
        SetError(error, "The label count must match the embedding vertex count");
        return false;
    }
    const std::uint64_t vertex_count = embedding.vertex_count;
    std::uint64_t sample_count = requested_sample_vertices;
    if (sample_count == 0) {
        sample_count = vertex_count / 10;
        if (sample_count == 0 && vertex_count > 1) sample_count = 1;
    } else {
        sample_count = std::min(sample_count, vertex_count);
    }
    result->classification_sample_count = sample_count;
    result->classification_training_count = static_cast<std::uint32_t>(vertex_count - sample_count);
    if (sample_count == 0 || sample_count == vertex_count) {
        result->has_classification = false;
        return true;
    }

    const std::uint32_t k_values[] = {1, 5, 10, 20, 30, 40, 50};
    std::uint64_t correct[7] = {0, 0, 0, 0, 0, 0, 0};
    std::vector<bool> held_out(static_cast<std::size_t>(vertex_count), false);
    for (std::uint64_t sample = 0; sample < sample_count; ++sample) {
        held_out[SampleVertex(sample, sample_count, vertex_count)] = true;
    }
    const std::size_t nearest_limit = std::min<std::size_t>(
        50, static_cast<std::size_t>(vertex_count - sample_count));
    std::vector<NeighborCandidate> nearest;
    nearest.reserve(nearest_limit);
    for (std::uint64_t sample = 0; sample < sample_count; ++sample) {
        const std::uint32_t source = SampleVertex(sample, sample_count, vertex_count);
        nearest.clear();
        for (std::uint32_t candidate = 0; candidate < embedding.vertex_count; ++candidate) {
            if (candidate == source || held_out[candidate]) continue;
            double squared_distance = 0.0;
            if (!SquaredDistance(embedding, source, candidate, &squared_distance)) {
                SetError(error, "Embedded distance calculation overflowed");
                return false;
            }
            KeepNearest(NeighborCandidate{squared_distance, candidate}, nearest_limit, &nearest);
        }
        std::sort(nearest.begin(), nearest.end(), CandidateLess);
        std::map<std::uint64_t, std::uint32_t> label_counts;
        for (std::size_t index = 0; index < nearest.size(); ++index) {
            ++label_counts[labels[nearest[index].vertex]];
            for (std::size_t metric = 0; metric < 7; ++metric) {
                if (index + 1 != std::min<std::size_t>(k_values[metric], nearest.size())) continue;
                std::uint64_t best_label = 0;
                std::uint32_t best_count = 0;
                bool have_label = false;
                for (std::map<std::uint64_t, std::uint32_t>::const_iterator item = label_counts.begin();
                     item != label_counts.end(); ++item) {
                    if (!have_label || item->second > best_count ||
                        (item->second == best_count && item->first < best_label)) {
                        best_label = item->first;
                        best_count = item->second;
                        have_label = true;
                    }
                }
                if (have_label && best_label == labels[source]) ++correct[metric];
            }
        }
    }
    const double denominator = static_cast<double>(sample_count);
    result->accuracy_1nn = correct[0] / denominator;
    result->accuracy_5nn = correct[1] / denominator;
    result->accuracy_10nn = correct[2] / denominator;
    result->accuracy_20nn = correct[3] / denominator;
    result->accuracy_30nn = correct[4] / denominator;
    result->accuracy_40nn = correct[5] / denominator;
    result->accuracy_50nn = correct[6] / denominator;
    result->has_classification = true;
    return true;
}
