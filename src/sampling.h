#ifndef SAMPLING_H
#define SAMPLING_H

#include "graph.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct AliasTable {
    std::vector<float> probability;
    std::vector<std::uint32_t> alias;

    std::size_t size() const;
};

class RandomGenerator {
public:
    explicit RandomGenerator(std::uint64_t seed)
        : state_(seed ? seed : 0x9e3779b97f4a7c15ULL) {}

    std::uint64_t Next() {
        state_ += 0x9e3779b97f4a7c15ULL;
        std::uint64_t value = state_;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    std::size_t Index(std::size_t bound) {
        return bound == 0 ? 0 : static_cast<std::size_t>(Next() % bound);
    }

    float UnitFloat() {
        return static_cast<float>((Next() >> 40) * (1.0 / 16777216.0));
    }

private:
    std::uint64_t state_;
};

bool BuildAliasTable(const std::vector<float>& weights, AliasTable* table, std::string* error);
bool BuildNegativeAliasTable(const CsrGraph& graph, AliasTable* table, std::string* error);
bool BuildPositiveAliasTables(const CsrGraph& graph,
                              AliasTable* source_table,
                              AliasTable* edge_table,
                              std::string* error);
bool BuildLayoutAliasTables(const CsrGraph& graph,
                            AliasTable* source_table,
                            AliasTable* edge_table,
                            AliasTable* negative_table,
                            std::string* error);
inline bool SampleAlias(const AliasTable& table, RandomGenerator* random, std::uint32_t* sample) {
    if (random == nullptr || sample == nullptr || table.probability.empty()) return false;
    const std::size_t column = random->Index(table.probability.size());
    *sample = random->UnitFloat() < table.probability[column]
        ? static_cast<std::uint32_t>(column) : table.alias[column];
    return true;
}

inline bool SamplePositiveEdge(const CsrGraph& graph,
                               const AliasTable& source_table,
                               const AliasTable& edge_table,
                               RandomGenerator* random,
                               std::uint32_t* source,
                               std::uint32_t* edge) {
    if (!SampleAlias(source_table, random, source) || *source >= graph.vertex_count) return false;
    const std::size_t begin = static_cast<std::size_t>(graph.row_offsets[*source]);
    const std::size_t end = static_cast<std::size_t>(graph.row_offsets[*source + 1]);
    if (begin >= end) return false;
    const std::size_t column = begin + random->Index(end - begin);
    if (edge_table.probability.empty()) {
        *edge = static_cast<std::uint32_t>(column);
        return true;
    }
    *edge = random->UnitFloat() < edge_table.probability[column]
        ? static_cast<std::uint32_t>(column) : edge_table.alias[column];
    return true;
}

#endif
