#include "knn.h"

#include "efanna/efanna.hpp"
#include "progress.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef DRGRAPH_HAVE_OPENMP
#include <omp.h>
#endif

namespace {

class EfannaThreadScope {
public:
    explicit EfannaThreadScope(std::uint32_t threads) : previous_(0) {
#ifdef DRGRAPH_HAVE_OPENMP
        previous_ = omp_get_max_threads();
        omp_set_num_threads(static_cast<int>(threads));
#else
        (void)threads;
#endif
    }

    ~EfannaThreadScope() {
#ifdef DRGRAPH_HAVE_OPENMP
        omp_set_num_threads(previous_);
#endif
    }

private:
    int previous_;
};

class EfannaLogScope {
public:
    EfannaLogScope() : previous_(std::cout.rdbuf(discard_.rdbuf())) {}
    ~EfannaLogScope() { std::cout.rdbuf(previous_); }

private:
    std::streambuf* previous_;
    std::ostringstream discard_;
};

void UpdateEfannaBuildProgress(void*, std::size_t completed, std::size_t) {
    ProgressBar::Update(static_cast<std::uint64_t>(completed));
}

bool HasAvx(std::string* error) {
#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_cpu_supports("avx")) return true;
    *error = "EFANNA requires AVX instructions";
    return false;
#endif
#endif
    *error = "This platform cannot confirm the AVX instructions required by EFANNA";
    return false;
}

bool FillResultRow(const DenseVectors& data,
                   std::uint32_t source,
                   const std::vector<unsigned>& raw_ids,
                   std::uint32_t neighbors,
                   KnnResult* result) {
    std::vector<std::pair<float, std::uint32_t> > candidates;
    candidates.reserve(raw_ids.size());
    const float* source_values = data.values.data() + static_cast<std::size_t>(source) * data.dimension;
    for (std::size_t index = 0; index < raw_ids.size(); ++index) {
        const std::uint32_t target = raw_ids[index];
        if (target == source || target >= data.point_count) continue;
        bool duplicate = false;
        for (std::size_t prior = 0; prior < candidates.size(); ++prior) {
            if (candidates[prior].second == target) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;
        const float* target_values = data.values.data() + static_cast<std::size_t>(target) * data.dimension;
        double distance = 0.0;
        for (std::uint64_t dimension = 0; dimension < data.dimension; ++dimension) {
            const double delta = static_cast<double>(source_values[dimension]) - target_values[dimension];
            distance += delta * delta;
        }
        if (!std::isfinite(distance) || distance > std::numeric_limits<float>::max()) {
            return false;
        }
        candidates.push_back(std::make_pair(static_cast<float>(distance), target));
    }
    if (candidates.size() < neighbors) {
        return false;
    }
    std::sort(candidates.begin(), candidates.end());
    const std::size_t offset = static_cast<std::size_t>(source) * neighbors;
    for (std::uint32_t index = 0; index < neighbors; ++index) {
        result->indices[offset + index] = candidates[index].second;
        result->squared_distances[offset + index] = candidates[index].first;
    }
    return true;
}

}  // namespace

namespace drgraph_internal {

const char* EfannaProfileNameInternal() {
    return "comparison-kdtree:trees=8,mlevel=8,epochs=8,buildTrees=8,S=k+1,L=max(30,S),checkK=max(25,S)";
}

bool BuildEfannaKnnInternal(const DenseVectors& data,
                    const KnnConfig& config,
                    std::uint32_t neighbors,
                    KnnResult* result,
                    std::string* error,
                    KnnStageStats* stats) {
    if (data.point_count < 4096) {
        *error = "The original EFANNA fixed parameters require at least 4096 vectors";
        return false;
    }
    if (config.deterministic && config.threads != 1) {
        *error = "Multithreaded EFANNA index construction is not deterministic; use --deterministic=0 or --threads=1";
        return false;
    }
    if (!HasAvx(error)) return false;
    if (neighbors >= static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        *error = "EFANNA candidate neighbor count exceeds the library interface limit";
        return false;
    }
    const int efanna_candidates = static_cast<int>(neighbors + 1U);
    const int efanna_list_size = std::max(30, efanna_candidates);
    const int efanna_check_size = std::max(25, efanna_candidates);
    const std::uint64_t stride = (data.dimension + 7ULL) & ~7ULL;
    if (stride < data.dimension || data.point_count > std::numeric_limits<std::size_t>::max() / stride) {
        *error = "Aligned EFANNA input matrix exceeds the current process size range";
        return false;
    }
    KnnResult built;
    built.point_count = data.point_count;
    built.neighbors = neighbors;
    built.indices.resize(static_cast<std::size_t>(data.point_count) * neighbors);
    built.squared_distances.resize(built.indices.size());
    bool build_progress_active = false;
    bool finalize_progress_active = false;
    try {
        std::vector<float> padded_values;
        const void* values = data.values.data();
        if (stride != data.dimension) {
            padded_values.assign(static_cast<std::size_t>(data.point_count * stride), 0.0f);
            for (std::uint32_t row = 0; row < data.point_count; ++row) {
                std::copy(data.values.begin() + static_cast<std::size_t>(row) * data.dimension,
                          data.values.begin() + static_cast<std::size_t>(row + 1) * data.dimension,
                          padded_values.begin() + static_cast<std::size_t>(row) * stride);
            }
            values = padded_values.data();
        }
        EfannaThreadScope thread_scope(config.threads);
        EfannaLogScope log_scope;
        efanna::Matrix<float> matrix(static_cast<std::size_t>(data.point_count),
                                     static_cast<std::size_t>(data.dimension), values);
        efanna::KDTreeUbIndexParams params(true, 8, 8, 8, efanna_check_size, efanna_list_size,
                                            static_cast<int>(neighbors), 8, efanna_candidates);
        params.build_progress_callback = UpdateEfannaBuildProgress;
        efanna::FIndex<float> index(
            matrix, new efanna::L2DistanceAVX<float>(),
            params);
        const std::chrono::steady_clock::time_point build_begin = std::chrono::steady_clock::now();
    ProgressBar::Begin("Compute EFANNA kNN", static_cast<std::uint64_t>(params.build_epoches + 1));
        build_progress_active = true;
        index.buildIndex();
        ProgressBar::Finish();
        build_progress_active = false;
        if (stats != nullptr) {
            stats->index_build_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - build_begin).count();
        }
        const std::chrono::steady_clock::time_point finalize_begin = std::chrono::steady_clock::now();
        std::atomic<std::uint32_t> failed_source(data.point_count);
        std::atomic<std::uint64_t> finalized_rows(0);
    ProgressBar::Begin("Finalize kNN results", data.point_count);
        finalize_progress_active = true;
#if defined(DRGRAPH_HAVE_OPENMP) && !defined(DRGRAPH_THREAD_SANITIZER)
#pragma omp parallel for schedule(static) num_threads(static_cast<int>(config.threads))
#endif
        for (std::int64_t source_index = 0; source_index < static_cast<std::int64_t>(data.point_count); ++source_index) {
            const std::uint32_t source = static_cast<std::uint32_t>(source_index);
            if (failed_source.load(std::memory_order_relaxed) != data.point_count) continue;
            if (!FillResultRow(data, source, index.getGraphCandidateRow(source), neighbors, &built)) {
                std::uint32_t expected = data.point_count;
                failed_source.compare_exchange_strong(expected, source, std::memory_order_relaxed);
            }
            const std::uint64_t completed = finalized_rows.fetch_add(1, std::memory_order_relaxed) + 1;
            if (completed == data.point_count || completed % 4096 == 0) ProgressBar::Update(completed);
        }
        if (failed_source.load(std::memory_order_relaxed) != data.point_count) {
            ProgressBar::Abort();
            finalize_progress_active = false;
            const std::uint32_t source = failed_source.load(std::memory_order_relaxed);
            const std::vector<unsigned> raw_ids = index.getGraphCandidateRow(source);
            std::ostringstream message;
                message << "EFANNA did not return enough neighbors after excluding the source: row " << source
                    << " has " << raw_ids.size() << " candidates";
            *error = message.str();
            return false;
        }
        ProgressBar::Finish();
        finalize_progress_active = false;
        if (stats != nullptr) {
            stats->result_finalize_seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - finalize_begin).count();
        }
    } catch (const std::exception& exception) {
        if (build_progress_active || finalize_progress_active) ProgressBar::Abort();
        *error = std::string("EFANNA kNN failed: ") + exception.what();
        return false;
    } catch (...) {
        if (build_progress_active || finalize_progress_active) ProgressBar::Abort();
        *error = "EFANNA kNN failed with an unknown exception";
        return false;
    }
    *result = std::move(built);
    return true;
}

}  // namespace drgraph_internal
