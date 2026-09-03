#ifndef VECTORS_H
#define VECTORS_H

#include <cstdint>
#include <vector>

struct DenseVectors {
    std::uint64_t point_count = 0;
    std::uint64_t dimension = 0;
    std::vector<float> values;
};

#endif
