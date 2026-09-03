#ifndef BINARY_IO_H
#define BINARY_IO_H

#include "graph.h"
#include "vectors.h"

#include <cstdint>
#include <string>
#include <vector>

enum class BinaryInputKind {
    Unknown,
    Data,
    Graph
};

bool ReadBinaryGraph(const std::string& path,
                     CsrGraph* graph,
                     std::string* error);
bool ReadBinaryData(const std::string& path, DenseVectors* data, std::string* error);
BinaryInputKind DetectBinaryInputKind(const std::string& path);

#endif
