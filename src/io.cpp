#include "io.h"
#include "progress.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace {

const char kBinaryMagic[] = "DRGBIN01";
const std::uint32_t kBinaryVersion = 1;
const std::uint32_t kBinaryData = 1;
const std::uint32_t kBinaryGraph = 2;
const std::uint64_t kBinaryHeaderBytes = 32;
const std::uint64_t kBinaryGraphRecordBytes = 12;
const std::uint64_t kProgressChunkBytes = 4ULL * 1024ULL * 1024ULL;

struct BinaryHeader {
    std::uint32_t version;
    std::uint32_t kind;
    std::uint64_t first_count;
    std::uint64_t second_count;
};

bool IsLittleEndianHost() {
    const std::uint16_t value = 1;
    return *reinterpret_cast<const unsigned char*>(&value) == 1;
}

std::uint32_t ByteSwap32(std::uint32_t value) {
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

std::uint64_t ByteSwap64(std::uint64_t value) {
    return (static_cast<std::uint64_t>(ByteSwap32(static_cast<std::uint32_t>(value))) << 32) |
           ByteSwap32(static_cast<std::uint32_t>(value >> 32));
}

bool CheckedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t* result) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) return false;
    *result = left + right;
    return true;
}

bool CheckedMultiply(std::uint64_t left, std::uint64_t right, std::uint64_t* result) {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) return false;
    *result = left * right;
    return true;
}

bool ReadExact(std::ifstream* input, char* destination, std::uint64_t bytes) {
    if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) return false;
    input->read(destination, static_cast<std::streamsize>(bytes));
    return input->good() || input->gcount() == static_cast<std::streamsize>(bytes);
}

bool ReadExactWithProgress(std::ifstream* input,
                           char* destination,
                           std::uint64_t bytes,
                           const char* label) {
    ProgressBar::Begin(label, bytes);
    std::uint64_t completed = 0;
    while (completed < bytes) {
        const std::uint64_t chunk = std::min(kProgressChunkBytes, bytes - completed);
        input->read(destination + completed, static_cast<std::streamsize>(chunk));
        if (!input->good() && input->gcount() != static_cast<std::streamsize>(chunk)) {
            ProgressBar::Abort();
            return false;
        }
        completed += chunk;
        ProgressBar::Update(completed);
    }
    ProgressBar::Finish();
    return true;
}

bool GetFileSize(std::ifstream* input, std::uint64_t* size) {
    input->seekg(0, std::ios::end);
    const std::streampos end = input->tellg();
    if (end < 0) return false;
    *size = static_cast<std::uint64_t>(end);
    input->seekg(0, std::ios::beg);
    return input->good();
}

bool ReadBinaryHeader(const std::string& path,
                      std::uint32_t expected_kind,
                      BinaryHeader* header,
                      std::uint64_t* file_size,
                      std::ifstream* input,
                      std::string* error) {
    input->open(path.c_str(), std::ios::binary);
    if (!*input) {
        *error = "Cannot open input file: " + path;
        return false;
    }
    if (!GetFileSize(input, file_size) || *file_size < kBinaryHeaderBytes) {
        *error = "Binary file " + path + " has an incomplete header";
        return false;
    }
    char magic[sizeof(kBinaryMagic) - 1] = {};
    if (!ReadExact(input, magic, sizeof(magic)) ||
        std::memcmp(magic, kBinaryMagic, sizeof(magic)) != 0 ||
        !ReadExact(input, reinterpret_cast<char*>(&header->version), sizeof(header->version)) ||
        !ReadExact(input, reinterpret_cast<char*>(&header->kind), sizeof(header->kind)) ||
        !ReadExact(input, reinterpret_cast<char*>(&header->first_count), sizeof(header->first_count)) ||
        !ReadExact(input, reinterpret_cast<char*>(&header->second_count), sizeof(header->second_count))) {
        *error = "Binary file " + path + " has an incomplete header";
        return false;
    }
    if (!IsLittleEndianHost()) {
        header->version = ByteSwap32(header->version);
        header->kind = ByteSwap32(header->kind);
        header->first_count = ByteSwap64(header->first_count);
        header->second_count = ByteSwap64(header->second_count);
    }
    if (header->version != kBinaryVersion) {
        *error = "Binary file " + path + " uses an unsupported version";
        return false;
    }
    if (header->kind != expected_kind) {
        *error = "Binary file " + path + " has a kind that does not match its extension";
        return false;
    }
    return true;
}

bool CheckBinarySize(const std::string& path,
                     std::uint64_t record_count,
                     std::uint64_t record_bytes,
                     std::uint64_t file_size,
                     std::string* error) {
    std::uint64_t payload_bytes = 0;
    std::uint64_t expected_bytes = 0;
    if (!CheckedMultiply(record_count, record_bytes, &payload_bytes) ||
        !CheckedAdd(kBinaryHeaderBytes, payload_bytes, &expected_bytes) ||
        expected_bytes != file_size) {
        *error = "Binary file " + path + " length does not match its header counts";
        return false;
    }
    return true;
}

static_assert(sizeof(Edge) == kBinaryGraphRecordBytes,
              "Binary graph edge records must be 12 bytes");

void ByteSwapEdge(Edge* edge) {
    edge->source = ByteSwap32(edge->source);
    edge->target = ByteSwap32(edge->target);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &edge->weight, sizeof(bits));
    bits = ByteSwap32(bits);
    std::memcpy(&edge->weight, &bits, sizeof(bits));
}

}  // namespace

BinaryInputKind DetectBinaryInputKind(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".data") == 0) {
        return BinaryInputKind::Data;
    }
    if (path.size() >= 6 && path.compare(path.size() - 6, 6, ".graph") == 0) {
        return BinaryInputKind::Graph;
    }
    return BinaryInputKind::Unknown;
}

static bool ReadBinaryDataHeader(const std::string& path,
                                 std::uint64_t* point_count,
                                 std::uint64_t* dimension,
                                 std::string* error) {
    BinaryHeader header;
    std::uint64_t file_size = 0;
    std::ifstream input;
    if (!ReadBinaryHeader(path, kBinaryData, &header, &file_size, &input, error)) return false;
    std::uint64_t value_count = 0;
    if (header.first_count == 0 || header.first_count > std::numeric_limits<std::uint32_t>::max() ||
        header.second_count == 0 ||
        header.first_count > std::numeric_limits<std::size_t>::max() / header.second_count ||
        !CheckedMultiply(header.first_count, header.second_count, &value_count) ||
        value_count > std::numeric_limits<std::size_t>::max() / sizeof(float) ||
        !CheckBinarySize(path, value_count, sizeof(float), file_size, error)) {
        if (error->empty()) *error = "Binary file " + path + " requires N in uint32_t range and positive D";
        return false;
    }
    *point_count = header.first_count;
    *dimension = header.second_count;
    return true;
}

bool ReadBinaryData(const std::string& path, DenseVectors* data, std::string* error) {
    std::uint64_t point_count = 0;
    std::uint64_t dimension = 0;
    if (!ReadBinaryDataHeader(path, &point_count, &dimension, error)) return false;
    std::uint64_t value_count = 0;
    if (!CheckedMultiply(point_count, dimension, &value_count)) {
        *error = "Binary file " + path + " N and D overflow the supported size";
        return false;
    }
    std::ifstream input(path.c_str(), std::ios::binary);
    input.seekg(static_cast<std::streamoff>(kBinaryHeaderBytes), std::ios::beg);
    DenseVectors parsed;
    parsed.point_count = point_count;
    parsed.dimension = dimension;
    parsed.values.resize(static_cast<std::size_t>(value_count));
    if (!ReadExactWithProgress(&input, reinterpret_cast<char*>(parsed.values.data()), value_count * sizeof(float), "Read .data")) {
        *error = "Binary file " + path + " has truncated vector data";
        return false;
    }
    if (!IsLittleEndianHost()) {
        for (std::size_t index = 0; index < parsed.values.size(); ++index) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &parsed.values[index], sizeof(bits));
            bits = ByteSwap32(bits);
            std::memcpy(&parsed.values[index], &bits, sizeof(bits));
        }
    }
    ProgressBar::Begin("Validate .data", parsed.values.size());
    for (std::size_t index = 0; index < parsed.values.size(); ++index) {
        if (!std::isfinite(parsed.values[index])) {
            ProgressBar::Abort();
            *error = "Binary file " + path + " contains a non-finite vector value";
            return false;
        }
        if ((index + 1) % (1024 * 1024) == 0 || index + 1 == parsed.values.size()) ProgressBar::Update(index + 1);
    }
    ProgressBar::Finish();
    *data = std::move(parsed);
    return true;
}

bool ReadBinaryGraph(const std::string& path,
                     CsrGraph* graph,
                     std::string* error) {
    BinaryHeader header;
    std::uint64_t file_size = 0;
    std::ifstream input;
    if (!ReadBinaryHeader(path, kBinaryGraph, &header, &file_size, &input, error)) return false;
    if (header.first_count == 0 || header.first_count > std::numeric_limits<std::uint32_t>::max() ||
        header.second_count > std::numeric_limits<std::size_t>::max() / sizeof(Edge) ||
        !CheckBinarySize(path, header.second_count, kBinaryGraphRecordBytes, file_size, error)) {
        if (error->empty()) *error = "Binary file " + path + " has invalid N or M";
        return false;
    }
    std::vector<Edge> edges;
    edges.resize(static_cast<std::size_t>(header.second_count));
    if (!edges.empty() && !ReadExactWithProgress(&input, reinterpret_cast<char*>(edges.data()),
                                                  header.second_count * kBinaryGraphRecordBytes, "Read .graph")) {
        *error = "Binary file " + path + " has truncated edge data";
        return false;
    }
    if (!IsLittleEndianHost()) {
        for (std::size_t record = 0; record < edges.size(); ++record) {
            ByteSwapEdge(&edges[record]);
        }
    }
    return BuildUndirectedCsr(header.first_count, &edges, graph, error);
}
