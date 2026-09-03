#include "optimizer.h"

#include "hierarchy.h"
#include "progress.h"
#include "sampling.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <thread>
#include <utility>

namespace {

bool CheckConfig(const LayoutConfig& config, std::string* error) {
    if (config.output_dimension == 0 || config.threads == 0 || config.samples == 0) {
        *error = "Layout dimension, thread count, and samples must be positive";
        return false;
    }
    return true;
}

class SpinLock {
public:
    SpinLock() : flag_(ATOMIC_FLAG_INIT) {}

    void Lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) std::this_thread::yield();
    }

    void Unlock() { flag_.clear(std::memory_order_release); }

private:
    std::atomic_flag flag_;
    char padding_[63];
};

static_assert(sizeof(SpinLock) == 64, "SpinLock should occupy one cache line");

class StripedLocks {
public:
    explicit StripedLocks(std::uint64_t vertex_count) : mask_(0) {
        std::size_t count = 1;
        const std::uint64_t limit = std::min<std::uint64_t>(vertex_count, 65536);
        while (count < limit) count <<= 1;
        storage_ = std::malloc(count * sizeof(SpinLock) + 63);
        if (storage_ == nullptr) throw std::bad_alloc();
        const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(storage_);
        locks_ = reinterpret_cast<SpinLock*>((raw + 63U) & ~static_cast<std::uintptr_t>(63U));
        for (std::size_t index = 0; index < count; ++index) new (locks_ + index) SpinLock();
        mask_ = count - 1;
    }

    ~StripedLocks() {
        if (locks_ != nullptr) {
            const std::size_t count = mask_ + 1;
            for (std::size_t index = 0; index < count; ++index) locks_[index].~SpinLock();
        }
        std::free(storage_);
    }

    StripedLocks(const StripedLocks&) = delete;
    StripedLocks& operator=(const StripedLocks&) = delete;

    SpinLock* ForVertex(std::uint32_t vertex) {
        return &locks_[(static_cast<std::size_t>(vertex) * 2654435761U) & mask_];
    }

private:
    void* storage_ = nullptr;
    SpinLock* locks_ = nullptr;
    std::size_t mask_;
};

class PairLock {
public:
    PairLock(StripedLocks* locks, std::uint32_t left, std::uint32_t right)
        : first_(locks->ForVertex(left)), second_(locks->ForVertex(right)) {
        if (second_ < first_) std::swap(first_, second_);
        first_->Lock();
        if (second_ != first_) second_->Lock();
    }

    ~PairLock() {
        if (second_ != first_) second_->Unlock();
        first_->Unlock();
    }

private:
    SpinLock* first_;
    SpinLock* second_;
};

float GradientForDistance(float squared_distance, float gamma, bool positive) {
    const float distance = std::max(0.0f, squared_distance);
    if (positive) return -2.0f / (1.0f + distance);
    return 2.0f * gamma / (1.0f + distance) / (0.1f + distance);
}

float Clip(float value) {
    return std::max(-1.0f, std::min(1.0f, value));
}

void OptimizePair(std::vector<float>* values,
                  std::uint32_t source,
                  std::uint32_t target,
                  std::uint32_t dimension,
                  float learning_rate,
                  float gamma,
                  bool positive) {
    float* source_values = &(*values)[static_cast<std::size_t>(source) * dimension];
    float* target_values = &(*values)[static_cast<std::size_t>(target) * dimension];
    float squared_distance = 0.0f;
    for (std::uint32_t coordinate = 0; coordinate < dimension; ++coordinate) {
        const float difference = source_values[coordinate] - target_values[coordinate];
        squared_distance += difference * difference;
    }
    const float gradient = GradientForDistance(squared_distance, gamma, positive);
    for (std::uint32_t coordinate = 0; coordinate < dimension; ++coordinate) {
        const float difference = source_values[coordinate] - target_values[coordinate];
        const float update = learning_rate * Clip(gradient * difference);
        source_values[coordinate] += update;
        target_values[coordinate] -= update;
    }
}

bool OptimizeLevel(const CsrGraph& graph,
                   const LayoutConfig& config,
                   const std::vector<float>* initial_values,
                   const std::vector<std::uint32_t>* fine_to_coarse,
                   Embedding* embedding,
                   std::string* error) {
    if (!CheckConfig(config, error)) return false;
    const std::size_t vertex_count = static_cast<std::size_t>(graph.vertex_count);
    if (vertex_count > std::numeric_limits<std::size_t>::max() / config.output_dimension) {
        *error = "Embedding array size exceeds the allocatable range";
        return false;
    }
    const std::size_t value_count = vertex_count * config.output_dimension;
    if (initial_values != nullptr && initial_values->size() != value_count) {
        *error = "Hierarchy initialization embedding size does not match";
        return false;
    }
    if (fine_to_coarse != nullptr && fine_to_coarse->size() != vertex_count) {
        *error = "Hierarchy mapping size does not match";
        return false;
    }
    Embedding built;
    built.vertex_count = graph.vertex_count;
    built.dimension = config.output_dimension;
    if (initial_values != nullptr) {
        built.values = *initial_values;
    } else {
        built.values.resize(value_count);
        RandomGenerator random(config.seed + 0xa0761d6478bd642fULL);
        for (std::size_t index = 0; index < built.values.size(); ++index) {
            built.values[index] = (random.UnitFloat() - 0.5f) * 0.02f;
        }
    }
    if (graph.weight.empty() || config.epochs == 0) {
        *embedding = std::move(built);
        return true;
    }

    AliasTable positive_sources;
    AliasTable positive_edges;
    ProgressBar::Begin("Prepare layout sampling", 2);
    AliasTable negatives;
    if (!BuildLayoutAliasTables(graph, &positive_sources, &positive_edges, &negatives, error)) {
        ProgressBar::Abort();
        return false;
    }
    ProgressBar::Update(1);
    ProgressBar::Finish();

    if (config.samples > 0 && graph.vertex_count >
        std::numeric_limits<std::size_t>::max() / config.samples) {
        *error = "Layout sample count exceeds the computable range";
        return false;
    }
    const std::size_t samples_per_epoch = static_cast<std::size_t>(graph.vertex_count) * config.samples;
    if (samples_per_epoch > 0 && config.epochs >
        std::numeric_limits<std::size_t>::max() / samples_per_epoch) {
        *error = "Layout sample count exceeds the computable range";
        return false;
    }
    const std::size_t total_samples = samples_per_epoch * config.epochs;
    ProgressBar::Begin("Optimize layout", config.epochs);
    for (std::uint32_t epoch = 0; epoch < config.epochs; ++epoch) {
        if (config.deterministic) {
            RandomGenerator random(config.seed + 0xe7037ed1a0b428dbULL * (epoch + 1));
            for (std::size_t sample = 0; sample < samples_per_epoch; ++sample) {
                std::uint32_t source = 0;
                std::uint32_t edge = 0;
                if (!SamplePositiveEdge(graph, positive_sources, positive_edges, &random, &source, &edge)) {
                    ProgressBar::Abort();
                    *error = "Positive-edge alias sampling failed";
                    return false;
                }
                const std::uint32_t target = graph.dst[edge];
                if (fine_to_coarse != nullptr && (*fine_to_coarse)[source] == (*fine_to_coarse)[target]) continue;
                const std::size_t completed_samples = static_cast<std::size_t>(epoch) * samples_per_epoch + sample;
                const float progress = total_samples == 0 ? 1.0f :
                    static_cast<float>(completed_samples) / static_cast<float>(total_samples + 1);
                const float learning_rate = config.alpha * std::max(0.0001f, 1.0f - progress);
                OptimizePair(&built.values, source, target, config.output_dimension,
                             learning_rate, config.gamma, true);
                for (std::uint32_t negative = 0; negative < config.negative; ++negative) {
                    std::uint32_t vertex = 0;
                    if (!SampleAlias(negatives, &random, &vertex)) continue;
                    if (vertex == source || vertex == target) continue;
                    if (fine_to_coarse != nullptr &&
                        ((*fine_to_coarse)[vertex] == (*fine_to_coarse)[source] ||
                         (*fine_to_coarse)[vertex] == (*fine_to_coarse)[target])) continue;
                    OptimizePair(&built.values, source, vertex, config.output_dimension,
                                 learning_rate, config.gamma, false);
                }
            }
            ProgressBar::Update(epoch + 1);
            continue;
        }

        StripedLocks locks(graph.vertex_count);
        const std::uint32_t worker_count = std::min<std::uint32_t>(
            config.threads, static_cast<std::uint32_t>(std::min<std::size_t>(samples_per_epoch, std::numeric_limits<std::uint32_t>::max())));
        std::vector<std::thread> workers;
        try {
            workers.reserve(worker_count);
            for (std::uint32_t worker = 0; worker < worker_count; ++worker) {
                workers.push_back(std::thread([&, worker]() {
                    RandomGenerator random(config.seed + 0x9e3779b97f4a7c15ULL * (epoch + 1) + worker);
                    const std::size_t samples_per_worker = samples_per_epoch / worker_count;
                    const std::size_t remainder = samples_per_epoch % worker_count;
                    const std::size_t begin = samples_per_worker * worker +
                        std::min<std::size_t>(worker, remainder);
                    const std::size_t end = samples_per_worker * (worker + 1) +
                        std::min<std::size_t>(worker + 1, remainder);
                    for (std::size_t sample = begin; sample < end; ++sample) {
                        std::uint32_t source = 0;
                        std::uint32_t edge = 0;
                        if (!SamplePositiveEdge(graph, positive_sources, positive_edges, &random, &source, &edge)) continue;
                        const std::uint32_t target = graph.dst[edge];
                        if (fine_to_coarse != nullptr && (*fine_to_coarse)[source] == (*fine_to_coarse)[target]) continue;
                        const std::size_t completed_samples = static_cast<std::size_t>(epoch) * samples_per_epoch + sample;
                        const float progress = total_samples == 0 ? 1.0f :
                            static_cast<float>(completed_samples) / static_cast<float>(total_samples + 1);
                        const float learning_rate = config.alpha * std::max(0.0001f, 1.0f - progress);
                        {
                            PairLock guard(&locks, source, target);
                            OptimizePair(&built.values, source, target, config.output_dimension,
                                         learning_rate, config.gamma, true);
                        }
                        for (std::uint32_t negative = 0; negative < config.negative; ++negative) {
                            std::uint32_t vertex = 0;
                            if (!SampleAlias(negatives, &random, &vertex) || vertex == source || vertex == target) continue;
                            if (fine_to_coarse != nullptr &&
                                ((*fine_to_coarse)[vertex] == (*fine_to_coarse)[source] ||
                                 (*fine_to_coarse)[vertex] == (*fine_to_coarse)[target])) continue;
                            PairLock guard(&locks, source, vertex);
                            OptimizePair(&built.values, source, vertex, config.output_dimension,
                                         learning_rate, config.gamma, false);
                        }
                    }
                }));
            }
        } catch (const std::exception& exception) {
            for (std::size_t worker = 0; worker < workers.size(); ++worker) workers[worker].join();
            ProgressBar::Abort();
            *error = std::string("Fast optimization thread creation failed: ") + exception.what();
            return false;
        }
        for (std::size_t worker = 0; worker < workers.size(); ++worker) workers[worker].join();
        ProgressBar::Update(epoch + 1);
    }
    for (std::size_t index = 0; index < built.values.size(); ++index) {
        if (!std::isfinite(built.values[index])) {
            ProgressBar::Abort();
        *error = "Layout optimization produced non-finite coordinates";
            return false;
        }
    }
    ProgressBar::Finish();
    *embedding = std::move(built);
    return true;
}

void ProjectEmbedding(const HierarchyLevel& fine,
                      const Embedding& coarse,
                      std::uint64_t seed,
                      std::vector<float>* projected) {
    const std::size_t dimension = coarse.dimension;
    projected->resize(static_cast<std::size_t>(fine.graph.vertex_count) * dimension);
    RandomGenerator random(seed);
    for (std::size_t vertex = 0; vertex < static_cast<std::size_t>(fine.graph.vertex_count); ++vertex) {
        const std::uint32_t cluster = fine.fine_to_coarse[vertex];
        std::vector<float> position(dimension, 0.0f);
        std::uint64_t count = 0;
        const std::size_t begin = static_cast<std::size_t>(fine.interpolation_offsets[vertex]);
        const std::size_t end = static_cast<std::size_t>(fine.interpolation_offsets[vertex + 1]);
        const std::uint32_t sun = fine.fine_sun[vertex];
        const std::uint32_t sun_cluster = fine.fine_to_coarse[sun];
        const float* sun_position = coarse.values.data() + static_cast<std::size_t>(sun_cluster) * dimension;
        for (std::size_t item = begin; item < end; ++item) {
            const std::uint32_t target_cluster = fine.fine_to_coarse[fine.interpolation_suns[item]];
            const float* target_position = coarse.values.data() + static_cast<std::size_t>(target_cluster) * dimension;
            const float lambda = fine.interpolation_lambda[item];
            const float radius = 0.05f * std::sqrt(std::inner_product(
                sun_position, sun_position + dimension, target_position, 0.0f,
                std::plus<float>(), [](float left, float right) {
                    const float difference = left - right;
                    return difference * difference;
                }));
            const float angle = random.UnitFloat() * 6.28318530718f;
            for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) {
                float value = sun_position[coordinate] + lambda * (target_position[coordinate] - sun_position[coordinate]);
                if (coordinate == 0) value += radius * std::cos(angle);
                else if (coordinate == 1) value += radius * std::sin(angle);
                position[coordinate] += value;
            }
            ++count;
        }
        if (count == 0) {
            for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) {
                position[coordinate] = coarse.values[static_cast<std::size_t>(cluster) * dimension + coordinate];
            }
        } else {
            for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) position[coordinate] /= count;
        }
        for (std::size_t coordinate = 0; coordinate < dimension; ++coordinate) {
            (*projected)[vertex * dimension + coordinate] = position[coordinate];
        }
    }
}

}  // namespace

bool OptimizeLayout(CsrGraph* graph,
                    const LayoutConfig& config,
                    Embedding* embedding,
                    std::string* error,
                    HierarchyStats* hierarchy_stats) {
    if (graph == nullptr || embedding == nullptr || error == nullptr) {
        if (error != nullptr) *error = "Layout input cannot be empty";
        return false;
    }
    if (!CheckConfig(config, error) ||
        !ValidateCsrGraph(*graph, error, CsrWeightPolicy::AllowZero)) return false;
    if (hierarchy_stats != nullptr) *hierarchy_stats = HierarchyStats();
    if (graph->vertex_count <= config.hierarchy_minimum_vertices || graph->weight.empty()) {
        return OptimizeLevel(*graph, config, nullptr, nullptr, embedding, error);
    }

    Hierarchy hierarchy;
    const std::chrono::steady_clock::time_point hierarchy_begin = std::chrono::steady_clock::now();
    if (!BuildHierarchy(std::move(*graph), config.hierarchy_minimum_vertices, &hierarchy, error, config.seed)) return false;
    struct RestoreFirstLevel {
        CsrGraph* destination;
        Hierarchy* source;
        ~RestoreFirstLevel() {
            if (destination != nullptr && source != nullptr && !source->levels.empty()) {
                *destination = std::move(source->levels.front().graph);
            }
        }
    } restore = {graph, &hierarchy};

    const std::uint32_t level_count = static_cast<std::uint32_t>(hierarchy.levels.size());
    LayoutConfig level_config = config;
    level_config.epochs = config.epochs == 0 ? 0 : std::max(1U, config.epochs / level_count);
    if (hierarchy_stats != nullptr) {
        hierarchy_stats->used = true;
        hierarchy_stats->build_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - hierarchy_begin).count();
        hierarchy_stats->epochs_per_level = level_config.epochs;
        for (std::size_t level = 0; level < hierarchy.levels.size(); ++level) {
            hierarchy_stats->vertex_counts.push_back(hierarchy.levels[level].graph.vertex_count);
            hierarchy_stats->arc_counts.push_back(hierarchy.levels[level].graph.weight.size());
        }
    }

    // The paper uses early exaggeration on the coarsest level and a medium
    // repulsive weight on all finer levels.
    level_config.gamma = 0.01f;
    Embedding current;
    level_config.samples = config.samples;
    if (!OptimizeLevel(hierarchy.levels.back().graph, level_config, nullptr, nullptr, &current, error)) return false;
    while (hierarchy.levels.size() > 1) {
        const std::size_t level = hierarchy.levels.size() - 1;
        HierarchyLevel& fine = hierarchy.levels[level - 1];
        std::vector<float> projected;
        ProjectEmbedding(fine, current, config.seed + 0xd1b54a32d192ed03ULL * level, &projected);
        hierarchy.levels.pop_back();
        level_config.seed = config.seed + level;
        level_config.gamma = config.gamma;
        level_config.samples = config.samples;
        const std::vector<std::uint32_t>* mapping = level == 1 ? nullptr : &fine.fine_to_coarse;
        if (!OptimizeLevel(fine.graph, level_config, &projected, mapping, &current, error)) return false;
        std::vector<std::uint32_t>().swap(fine.fine_to_coarse);
    }
    *embedding = std::move(current);
    return true;
}
