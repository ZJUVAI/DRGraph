#!/usr/bin/env python3
"""Convert strict text .data or .graph input to DRGBIN01 binary."""

import argparse
import math
import os
import struct
import sys

MAGIC = b"DRGBIN01"
VERSION = 1
DATA = 1
GRAPH = 2
HEADER = struct.Struct("<8sIIQQ")
DATA_VALUE = struct.Struct("<f")
GRAPH_EDGE = struct.Struct("<IIf")


def fail(message):
    raise ValueError(message)


def parse_counts(line, path, names):
    fields = line.split()
    if len(fields) != 2:
        fail(f"{path}: first line must contain {' '.join(names)}")
    if not all(field.isascii() and field.isdecimal() for field in fields):
        fail(f"{path}: first line must contain decimal integers")
    values = [int(field, 10) for field in fields]
    if any(value < 0 for value in values):
        fail(f"{path}: first line cannot contain negative values")
    return values


def parse_uint32(field, path, row):
    if not field.isascii() or not field.isdecimal():
        fail(f"{path}: vertex ID in edge {row} must be a decimal integer")
    value = int(field, 10)
    if value > 0xffffffff:
        fail(f"{path}: vertex ID in edge {row} exceeds uint32 range")
    return value


def pack_float32(value, path, description):
    try:
        packed = DATA_VALUE.pack(value)
    except (OverflowError, struct.error):
        fail(f"{path}: {description} exceeds float32 range")
    converted = DATA_VALUE.unpack(packed)[0]
    if not math.isfinite(converted):
        fail(f"{path}: {description} exceeds float32 range")
    return packed


def write_data(source, destination):
    first = source.readline()
    if not first:
        fail("Input is empty")
    count, dimension = parse_counts(first, source.name, ("N", "D"))
    if count == 0 or dimension == 0:
        fail(".data N and D must be positive")
    destination.write(HEADER.pack(MAGIC, VERSION, DATA, count, dimension))
    for row in range(count):
        line = source.readline()
        if not line:
            fail(f"{source.name}: missing vector row {row + 1}")
        fields = line.split()
        if len(fields) != dimension:
            fail(f"{source.name}: invalid dimension in row {row + 1}")
        for field in fields:
            value = float(field)
            if not math.isfinite(value):
                fail(f"{source.name}: contains a non-finite vector value")
            destination.write(pack_float32(value, source.name, "Vector value"))
    if any(line.strip() for line in source):
        fail(f"{source.name}: contains trailing content")


def write_graph(source, destination):
    first = source.readline()
    if not first:
        fail("Input is empty")
    vertices, edges = parse_counts(first, source.name, ("N", "M"))
    if vertices == 0 or vertices > 0xffffffff:
        fail(".graph N must fit in uint32")
    destination.write(HEADER.pack(MAGIC, VERSION, GRAPH, vertices, edges))
    for row in range(edges):
        line = source.readline()
        if not line:
            fail(f"{source.name}: missing edge {row + 1}")
        fields = line.split()
        if len(fields) not in (2, 3):
            fail(f"{source.name}: invalid field count in edge {row + 1}")
        source_id = parse_uint32(fields[0], source.name, row + 1)
        target_id = parse_uint32(fields[1], source.name, row + 1)
        weight = float(fields[2]) if len(fields) == 3 else 1.0
        if not (0 <= source_id < vertices and 0 <= target_id < vertices):
            fail(f"{source.name}: vertex ID is out of range")
        if source_id == target_id or not math.isfinite(weight) or weight <= 0:
            fail(f"{source.name}: edge is invalid")
        packed_weight = pack_float32(weight, source.name, "Edge weight")
        converted_weight = DATA_VALUE.unpack(packed_weight)[0]
        if converted_weight <= 0:
            fail(f"{source.name}: edge weight is below the smallest positive float32")
        destination.write(struct.pack("<II", source_id, target_id))
        destination.write(packed_weight)
    if any(line.strip() for line in source):
        fail(f"{source.name}: contains trailing content")


def main():
    parser = argparse.ArgumentParser(description="Convert strict text input to DRGBIN01")
    parser.add_argument("input", help="Input .data or .graph")
    parser.add_argument("output", help="Output .data or .graph")
    args = parser.parse_args()
    extension = os.path.splitext(args.input)[1]
    if extension not in (".data", ".graph") or os.path.splitext(args.output)[1] != extension:
        fail("Input and output must use the same .data or .graph extension")
    temporary = args.output + ".partial"
    try:
        with open(args.input, "rt", encoding="utf-8", newline="") as source, open(temporary, "wb") as destination:
            if extension == ".data":
                write_data(source, destination)
            else:
                write_graph(source, destination)
        os.replace(temporary, args.output)
    except Exception as error:
        if os.path.exists(temporary):
            os.remove(temporary)
        print(f"Conversion failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
