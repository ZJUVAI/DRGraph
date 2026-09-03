#include "knn.h"

#include "progress.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#ifdef DRGRAPH_HAVE_OPENMP
#include <omp.h>
#endif

#ifdef DRGRAPH_HAVE_EFANNA
namespace drgraph_internal {
bool BuildEfannaKnnInternal(const DenseVectors& data,
                            const KnnConfig& config,
                            std::uint32_t neighbors,
                            KnnResult* result,
                            std::string* error,
                            KnnStageStats* stats);
const char* EfannaProfileNameInternal();
}  // namespace drgraph_internal
#endif

namespace {

const std::uint64_t kExactKnnLimit = 4096;

bool CheckInput(const DenseVectors& data, const KnnConfig& config, std::string* error) {
    if (data.point_count < 2 || data.dimension == 0 || data.point_count > std::numeric_limits<std::uint32_t>::max() ||
        data.point_count > std::numeric_limits<std::size_t>::max() / data.dimension ||
        data.values.size() != static_cast<std::size_t>(data.point_count * data.dimension)) {
        *error = "kNN input must contain at least two points with valid matrix dimensions";
        return false;
    }
    if (config.neighbors == 0 || config.threads == 0) {
        *error = "kNN neighbor count and thread count must be positive";
        return false;
    }
    return true;
}

bool BuildExactKnn(const DenseVectors& data,
                   const KnnConfig& config,
                   std::uint32_t neighbors,
                   KnnResult* result,
                   std::string* error,
                   KnnStageStats* stats) {
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    KnnResult built;
    built.point_count = data.point_count;
    built.neighbors = neighbors;
    built.indices.resize(static_cast<std::size_t>(data.point_count) * neighbors);
    built.squared_distances.resize(built.indices.size());
    std::atomic<bool> overflow(false);
    std::atomic<std::uint64_t> completed_rows(0);
    const std::uint64_t update_interval = std::max<std::uint64_t>(1, data.point_count / 100);
    ProgressBar::Begin("Compute exact kNN", data.point_count);
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(static_cast<int>(config.threads))
#endif
    for (std::int64_t source_index = 0; source_index < static_cast<std::int64_t>(data.point_count); ++source_index) {
        const std::uint32_t source = static_cast<std::uint32_t>(source_index);
        std::vector<std::pair<float, std::uint32_t> > candidates;
        candidates.reserve(neighbors);
        const float* source_values = data.values.data() + static_cast<std::size_t>(source) * data.dimension;
        for (std::uint32_t target = 0; target < data.point_count; ++target) {
            if (source == target) continue;
            const float* target_values = data.values.data() + static_cast<std::size_t>(target) * data.dimension;
            double distance = 0.0;
            for (std::uint64_t dimension = 0; dimension < data.dimension; ++dimension) {
                const double difference = static_cast<double>(source_values[dimension]) - target_values[dimension];
                distance += difference * difference;
            }
            if (!std::isfinite(distance) || distance > std::numeric_limits<float>::max()) {
                overflow.store(true, std::memory_order_relaxed);
                break;
            }
            const std::pair<float, std::uint32_t> candidate(static_cast<float>(distance), target);
            if (candidates.size() < neighbors) {
                candidates.push_back(candidate);
                if (candidates.size() == neighbors) std::make_heap(candidates.begin(), candidates.end());
            } else if (candidate < candidates.front()) {
                std::pop_heap(candidates.begin(), candidates.end());
                candidates.back() = candidate;
                std::push_heap(candidates.begin(), candidates.end());
            }
        }
        if (candidates.size() == neighbors) {
            std::sort(candidates.begin(), candidates.end());
            for (std::uint32_t index = 0; index < neighbors; ++index) {
                const std::size_t offset = static_cast<std::size_t>(source) * neighbors + index;
                built.indices[offset] = candidates[index].second;
                built.squared_distances[offset] = candidates[index].first;
            }
        }
        const std::uint64_t completed = completed_rows.fetch_add(1, std::memory_order_relaxed) + 1;
        if (completed == data.point_count || completed % update_interval == 0) ProgressBar::Update(completed);
    }
    if (overflow.load(std::memory_order_relaxed)) {
        ProgressBar::Abort();
        *error = "Exact kNN squared L2 distance exceeds float range";
        return false;
    }
    ProgressBar::Finish();
    *result = std::move(built);
    if (stats != nullptr) {
        stats->search_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    }
    return true;
}

}  // namespace

bool UsesExactKnn(std::uint64_t point_count) {
    return point_count < kExactKnnLimit;
}

bool BuildKnn(const DenseVectors& data,
              const KnnConfig& config,
              KnnResult* result,
              std::string* error,
              KnnResolvedConfig* resolved_config,
              KnnStageStats* stage_stats) {
    if (result == nullptr || error == nullptr) return false;
    if (resolved_config != nullptr) *resolved_config = KnnResolvedConfig();
    if (stage_stats != nullptr) *stage_stats = KnnStageStats();
    if (!CheckInput(data, config, error)) return false;
    const std::uint32_t neighbors = std::min<std::uint32_t>(config.neighbors,
        static_cast<std::uint32_t>(data.point_count - 1));

    bool success = false;
    KnnResolvedConfig resolved;
    resolved.neighbors = neighbors;
    resolved.threads = config.threads;
    resolved.deterministic = config.deterministic;
    if (UsesExactKnn(data.point_count)) {
        resolved.backend = "exact";
        success = BuildExactKnn(data, config, neighbors, result, error, stage_stats);
    } else {
#ifdef DRGRAPH_HAVE_EFANNA
        resolved.backend = "efanna";
        resolved.efanna_profile = drgraph_internal::EfannaProfileNameInternal();
        success = drgraph_internal::BuildEfannaKnnInternal(data, config, neighbors, result, error, stage_stats);
#else
        *error = "This build has no EFANNA support for .data inputs with at least 4096 vectors";
        return false;
#endif
    }
    if (success && resolved_config != nullptr) *resolved_config = resolved;
    return success;
}
