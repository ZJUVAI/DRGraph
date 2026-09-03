#include "hierarchy.h"

#include "progress.h"
#include "sampling.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace {

const std::uint32_t kUnassigned = std::numeric_limits<std::uint32_t>::max();

struct CoarseEdge {
    std::uint32_t source;
    std::uint32_t target;
    float weight;
    std::uint32_t count;
};

bool CoarseEdgeLess(const CoarseEdge& left, const CoarseEdge& right) {
    if (left.source != right.source) return left.source < right.source;
    return left.target < right.target;
}

bool SetError(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

void RemoveAvailable(std::uint32_t vertex,
                     std::vector<std::uint32_t>* available,
                     std::vector<std::uint32_t>* position) {
    const std::uint32_t index = (*position)[vertex];
    if (index == kUnassigned) return;
    const std::uint32_t last = available->back();
    (*available)[index] = last;
    (*position)[last] = index;
    available->pop_back();
    (*position)[vertex] = kUnassigned;
}

std::uint32_t ChooseSun(const std::vector<std::uint32_t>& available,
                        const std::vector<std::uint32_t>& mass,
                        RandomGenerator* random) {
    std::uint32_t chosen = available[0];
    std::uint32_t chosen_mass = std::numeric_limits<std::uint32_t>::max();
    const std::size_t tries = std::min<std::size_t>(3, available.size());
    for (std::size_t attempt = 0; attempt < tries; ++attempt) {
        const std::uint32_t candidate = available[random->Index(available.size())];
        if (mass[candidate] < chosen_mass ||
            (mass[candidate] == chosen_mass && candidate < chosen)) {
            chosen = candidate;
            chosen_mass = mass[candidate];
        }
    }
    return chosen;
}

bool GroupFM3(const CsrGraph& graph,
              const std::vector<std::uint32_t>& mass,
              std::uint64_t seed,
              std::vector<std::uint32_t>* mapping,
              std::vector<unsigned char>* node_type,
              std::vector<std::uint32_t>* sun,
              std::vector<float>* distance,
              std::uint32_t* coarse_count) {
    const std::size_t vertices = static_cast<std::size_t>(graph.vertex_count);
    mapping->assign(vertices, kUnassigned);
    node_type->assign(vertices, 0);
    sun->assign(vertices, kUnassigned);
    distance->assign(vertices, 0.0f);
    std::vector<std::uint32_t> available;
    std::vector<std::uint32_t> position(vertices, kUnassigned);
    available.reserve(vertices);
    for (std::uint32_t vertex = 0; vertex < graph.vertex_count; ++vertex) {
        position[vertex] = vertex;
        available.push_back(vertex);
    }

    RandomGenerator random(seed);
    std::uint32_t cluster = 0;
    while (!available.empty()) {
        const std::uint32_t current = ChooseSun(available, mass, &random);
        RemoveAvailable(current, &available, &position);
        (*mapping)[current] = cluster;
        (*node_type)[current] = 1;
        (*sun)[current] = current;

        std::vector<std::uint32_t> planets;
        const std::uint64_t begin = graph.row_offsets[current];
        const std::uint64_t end = graph.row_offsets[current + 1];
        for (std::uint64_t edge = begin; edge < end; ++edge) {
            const std::size_t index = static_cast<std::size_t>(edge);
            const std::uint32_t vertex = graph.dst[index];
            if ((*node_type)[vertex] == 1) continue;
            (*mapping)[vertex] = cluster;
            (*node_type)[vertex] = 2;
            (*sun)[vertex] = current;
            (*distance)[vertex] = graph.weight[index];
            planets.push_back(vertex);
            RemoveAvailable(vertex, &available, &position);
        }
        for (std::size_t item = 0; item < planets.size(); ++item) {
            const std::uint32_t planet = planets[item];
            for (std::uint64_t edge = graph.row_offsets[planet];
                 edge < graph.row_offsets[planet + 1]; ++edge) {
                RemoveAvailable(graph.dst[static_cast<std::size_t>(edge)], &available, &position);
            }
        }
        ++cluster;
    }

    for (std::uint32_t vertex = 0; vertex < graph.vertex_count; ++vertex) {
        if ((*mapping)[vertex] != kUnassigned) continue;
        std::uint32_t anchor = kUnassigned;
        float best_weight = std::numeric_limits<float>::max();
        for (std::uint64_t edge = graph.row_offsets[vertex];
             edge < graph.row_offsets[vertex + 1]; ++edge) {
            const std::size_t index = static_cast<std::size_t>(edge);
            const std::uint32_t candidate = graph.dst[index];
            if ((*node_type)[candidate] != 2 && (*node_type)[candidate] != 3) continue;
            if (graph.weight[index] < best_weight ||
                (graph.weight[index] == best_weight && candidate < anchor)) {
                anchor = candidate;
                best_weight = graph.weight[index];
            }
        }
        if (anchor == kUnassigned) {
            (*mapping)[vertex] = cluster;
            (*node_type)[vertex] = 1;
            (*sun)[vertex] = vertex;
            ++cluster;
            continue;
        }
        (*node_type)[anchor] = 3;
        (*mapping)[vertex] = (*mapping)[anchor];
        (*node_type)[vertex] = 4;
        (*sun)[vertex] = (*sun)[anchor];
        (*distance)[vertex] = best_weight + (*distance)[anchor];
    }
    *coarse_count = cluster;
    return true;
}

bool BuildCoarseLevel(HierarchyLevel* fine,
                      const std::vector<std::uint32_t>& mapping,
                      const std::vector<std::uint32_t>& sun,
                      const std::vector<float>& distance_to_sun,
                      std::uint32_t coarse_count,
                      HierarchyLevel* coarse,
                      std::vector<std::uint32_t>* coarse_mass,
                      std::string* error) {
    const std::size_t vertices = static_cast<std::size_t>(fine->graph.vertex_count);
    const std::size_t coarse_vertices = static_cast<std::size_t>(coarse_count);
    HierarchyLevel built;
    built.graph.vertex_count = coarse_count;
    built.graph.row_offsets.assign(coarse_vertices + 1, 0);
    coarse_mass->assign(coarse_vertices, 0);
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
        ++(*coarse_mass)[mapping[vertex]];
    }

    std::vector<CoarseEdge> edges;
    edges.reserve(fine->graph.dst.size());
    fine->interpolation_offsets.assign(vertices + 1, 0);
    fine->interpolation_suns.clear();
    fine->interpolation_lambda.clear();
    for (std::uint32_t source = 0; source < fine->graph.vertex_count; ++source) {
        const std::uint32_t coarse_source = mapping[source];
        fine->interpolation_offsets[source] = fine->interpolation_suns.size();
        for (std::uint64_t edge = fine->graph.row_offsets[source];
             edge < fine->graph.row_offsets[source + 1]; ++edge) {
            const std::size_t index = static_cast<std::size_t>(edge);
            const std::uint32_t target = fine->graph.dst[index];
            const std::uint32_t coarse_target = mapping[target];
            if (coarse_source == coarse_target) continue;
            const float length = fine->graph.weight[index] +
                distance_to_sun[source] + distance_to_sun[target];
            if (!std::isfinite(length) || length <= 0.0f) {
                return SetError(error, "FM3 edge length is invalid");
            }
            edges.push_back(CoarseEdge{coarse_source, coarse_target, length, 1});
            fine->interpolation_suns.push_back(sun[target]);
            fine->interpolation_lambda.push_back(distance_to_sun[source] / length);
        }
    }
    fine->interpolation_offsets[vertices] = fine->interpolation_suns.size();

    std::sort(edges.begin(), edges.end(), CoarseEdgeLess);
    std::size_t merged = 0;
    for (std::size_t edge = 0; edge < edges.size(); ++edge) {
        if (merged == 0 || edges[merged - 1].source != edges[edge].source ||
            edges[merged - 1].target != edges[edge].target) {
            edges[merged++] = edges[edge];
            continue;
        }
        const double sum = static_cast<double>(edges[merged - 1].weight) + edges[edge].weight;
        if (!std::isfinite(sum) || sum > std::numeric_limits<float>::max()) {
                return SetError(error, "FM3 aggregated edge weight exceeds float range");
        }
        edges[merged - 1].weight = static_cast<float>(sum);
        ++edges[merged - 1].count;
    }
    edges.resize(merged);
    for (std::size_t edge = 0; edge < edges.size(); ++edge) {
        ++built.graph.row_offsets[edges[edge].source + 1];
    }
    for (std::size_t vertex = 1; vertex < built.graph.row_offsets.size(); ++vertex) {
        built.graph.row_offsets[vertex] += built.graph.row_offsets[vertex - 1];
    }
    built.graph.dst.resize(edges.size());
    built.graph.weight.resize(edges.size());
    for (std::size_t edge = 0; edge < edges.size(); ++edge) {
        built.graph.dst[edge] = edges[edge].target;
        built.graph.weight[edge] = edges[edge].weight /
            static_cast<float>(edges[edge].count);
    }
    *coarse = std::move(built);
    return true;
}

}  // namespace

bool BuildHierarchy(CsrGraph&& probability_graph,
                    std::uint32_t minimum_coarse_vertices,
                    Hierarchy* hierarchy,
                    std::string* error,
                    std::uint64_t seed) {
    if (hierarchy == nullptr) return SetError(error, "Hierarchy output is null");
    if (minimum_coarse_vertices == 0) return SetError(error, "Minimum coarse vertex count must be positive");
    if (!ValidateCsrGraph(probability_graph, error, CsrWeightPolicy::RequirePositive)) return false;

    Hierarchy built;
    HierarchyLevel first;
    first.graph = std::move(probability_graph);
    built.levels.push_back(std::move(first));
    std::vector<std::uint32_t> current_mass(
        static_cast<std::size_t>(built.levels.front().graph.vertex_count), 1);
    const std::uint64_t initial_vertices = built.levels.front().graph.vertex_count;
    ProgressBar::Begin("Build hierarchy", initial_vertices);

    while (built.levels.back().graph.vertex_count > minimum_coarse_vertices) {
        HierarchyLevel& fine = built.levels.back();
        std::vector<std::uint32_t> mapping;
        std::vector<unsigned char> node_type;
        std::vector<std::uint32_t> sun;
        std::vector<float> distance;
        std::uint32_t coarse_count = 0;
        if (!GroupFM3(fine.graph, current_mass, seed + built.levels.size(), &mapping,
                      &node_type, &sun, &distance, &coarse_count)) {
            ProgressBar::Abort();
            return false;
        }
        if (coarse_count >= fine.graph.vertex_count) break;
        if (built.levels.size() > 1 &&
            (static_cast<double>(coarse_count) / fine.graph.vertex_count > 0.7 ||
             coarse_count < minimum_coarse_vertices * 0.8)) break;

        std::vector<unsigned char>().swap(node_type);
        std::vector<std::uint32_t>().swap(current_mass);
        HierarchyLevel coarse;
        std::vector<std::uint32_t> coarse_mass;
        if (!BuildCoarseLevel(&fine, mapping, sun, distance, coarse_count,
                              &coarse, &coarse_mass, error)) {
            ProgressBar::Abort();
            return false;
        }
        fine.fine_to_coarse = std::move(mapping);
        fine.fine_sun = std::move(sun);
        std::vector<float>().swap(distance);
        built.levels.push_back(std::move(coarse));
        current_mass = std::move(coarse_mass);
        ProgressBar::Update(initial_vertices - built.levels.back().graph.vertex_count);
    }
    *hierarchy = std::move(built);
    ProgressBar::Finish();
    return true;
}
