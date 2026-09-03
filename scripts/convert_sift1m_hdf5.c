#include <hdf5.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kPointCount = 1000000,
    kDimension = 128,
    kBatchRows = 8192,
};

static int CheckDistanceAttribute(hid_t file) {
    if (H5Aexists(file, "distance") <= 0) return 0;
    hid_t attribute = H5Aopen(file, "distance", H5P_DEFAULT);
    hid_t type = attribute < 0 ? -1 : H5Aget_type(attribute);
    if (type < 0 || H5Tget_class(type) != H5T_STRING) {
        if (type >= 0) H5Tclose(type);
        if (attribute >= 0) H5Aclose(attribute);
        return 0;
    }
    int valid = 0;
    if (H5Tis_variable_str(type) > 0) {
        char* value = NULL;
        if (H5Aread(attribute, type, &value) >= 0 && value != NULL) valid = strcmp(value, "euclidean") == 0;
        if (value != NULL) H5free_memory(value);
    } else {
        const size_t length = H5Tget_size(type);
        char* value = malloc(length + 1);
        if (value != NULL) {
            memset(value, 0, length + 1);
            if (H5Aread(attribute, type, value) >= 0) valid = strcmp(value, "euclidean") == 0;
            free(value);
        }
    }
    H5Tclose(type);
    H5Aclose(attribute);
    return valid;
}

static int WriteHeader(FILE* output) {
    const char magic[8] = {'D', 'R', 'G', 'B', 'I', 'N', '0', '1'};
    const uint32_t version = 1;
    const uint32_t kind = 1;
    const uint64_t point_count = kPointCount;
    const uint64_t dimension = kDimension;
    const uint16_t endian = 1;
    if (*(const unsigned char*)&endian != 1) return 0;
    return fwrite(magic, sizeof(magic), 1, output) == 1 &&
           fwrite(&version, sizeof(version), 1, output) == 1 &&
           fwrite(&kind, sizeof(kind), 1, output) == 1 &&
           fwrite(&point_count, sizeof(point_count), 1, output) == 1 &&
           fwrite(&dimension, sizeof(dimension), 1, output) == 1;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input HDF5> <output .data>\n", argv[0]);
        return 64;
    }
    const char* input_path = argv[1];
    const char* output_path = argv[2];
    const size_t output_length = strlen(output_path) + sizeof(".partial");
    char* temporary_path = malloc(output_length);
    if (temporary_path == NULL) return 70;
    snprintf(temporary_path, output_length, "%s.partial", output_path);

    hid_t file = H5Fopen(input_path, H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t dataset = -1;
    hid_t file_space = -1;
    hid_t type = -1;
    FILE* output = NULL;
    float* buffer = NULL;
    int result = 1;
    if (file < 0 || !CheckDistanceAttribute(file)) {
        fprintf(stderr, "SIFT1M HDF5 must declare distance=euclidean\n");
        goto cleanup;
    }
    dataset = H5Dopen2(file, "train", H5P_DEFAULT);
    file_space = dataset < 0 ? -1 : H5Dget_space(dataset);
    type = dataset < 0 ? -1 : H5Dget_type(dataset);
    if (dataset < 0 || file_space < 0 || type < 0 || H5Tget_class(type) != H5T_FLOAT || H5Tget_size(type) != 4 ||
        H5Sget_simple_extent_ndims(file_space) != 2) {
        fprintf(stderr, "SIFT1M HDF5 train must be a two-dimensional float32 dataset\n");
        goto cleanup;
    }
    hsize_t dimensions[2] = {0, 0};
    if (H5Sget_simple_extent_dims(file_space, dimensions, NULL) < 0 ||
        dimensions[0] != kPointCount || dimensions[1] != kDimension) {
        fprintf(stderr, "SIFT1M HDF5 train must have shape (1000000, 128)\n");
        goto cleanup;
    }
    output = fopen(temporary_path, "wb");
    buffer = malloc((size_t)kBatchRows * kDimension * sizeof(float));
    if (output == NULL || buffer == NULL || !WriteHeader(output)) {
        fprintf(stderr, "Cannot create SIFT1M binary output\n");
        goto cleanup;
    }
    for (uint64_t row = 0; row < kPointCount; row += kBatchRows) {
        const hsize_t count[2] = {(hsize_t)((kPointCount - row) < kBatchRows ? (kPointCount - row) : kBatchRows), kDimension};
        const hsize_t start[2] = {row, 0};
        hid_t memory_space = H5Screate_simple(2, count, NULL);
        if (memory_space < 0 || H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, NULL, count, NULL) < 0 ||
            H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space, file_space, H5P_DEFAULT, buffer) < 0) {
            if (memory_space >= 0) H5Sclose(memory_space);
            fprintf(stderr, "Failed to read SIFT1M HDF5 train\n");
            goto cleanup;
        }
        H5Sclose(memory_space);
        const size_t values = (size_t)count[0] * kDimension;
        for (size_t index = 0; index < values; ++index) {
            if (!isfinite(buffer[index])) {
                fprintf(stderr, "SIFT1M HDF5 train contains a non-finite value\n");
                goto cleanup;
            }
        }
        if (fwrite(buffer, sizeof(float), values, output) != values) {
            fprintf(stderr, "Failed to write SIFT1M binary output\n");
            goto cleanup;
        }
    }
    if (fclose(output) != 0 || rename(temporary_path, output_path) != 0) {
        output = NULL;
        fprintf(stderr, "Failed to complete SIFT1M binary output\n");
        goto cleanup;
    }
    output = NULL;
    result = 0;

cleanup:
    if (output != NULL) fclose(output);
    if (result != 0) remove(temporary_path);
    free(buffer);
    if (type >= 0) H5Tclose(type);
    if (file_space >= 0) H5Sclose(file_space);
    if (dataset >= 0) H5Dclose(dataset);
    if (file >= 0) H5Fclose(file);
    free(temporary_path);
    return result;
}
