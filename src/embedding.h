#ifndef EMBEDDING_H
#define EMBEDDING_H

#include <cstdint>
#include <vector>

struct Embedding {
    std::uint64_t vertex_count = 0;
    std::uint32_t dimension = 0;
    std::vector<float> values;
};

#endif
