#!/usr/bin/env python3
"""Convert supported public graph sources to a validated DRGBIN01 graph."""

import argparse
import gzip
import math
import os
import struct
import tarfile
from pathlib import Path

MAGIC = b"DRGBIN01"
HEADER = struct.Struct("<8sIIQQ")
EDGE = struct.Struct("<IIf")


def fail(message):
    raise ValueError(message)


def parse_edge(fields, row, vertices, weighted=True, one_based=False):
    if len(fields) not in (2, 3):
        fail(f"edge {row}: expected two or three fields")
    source, target = int(fields[0]), int(fields[1])
    if one_based:
        source -= 1
        target -= 1
    if not (0 <= source < vertices and 0 <= target < vertices):
        fail(f"edge {row}: vertex ID is out of range")
    if source == target:
        fail(f"edge {row}: self-loops are not supported")
    weight = float(fields[2]) if weighted and len(fields) == 3 else 1.0
    if not math.isfinite(weight) or weight <= 0:
        fail(f"edge {row}: weight must be finite and positive")
    return source, target, weight


def parse_text(path):
    with open(path, "rt", encoding="utf-8") as source:
        first = source.readline().split()
        if len(first) != 2:
            fail("text graph must start with N M")
        vertices, edge_count = map(int, first)
        if vertices <= 0 or vertices > 0xffffffff or edge_count < 0:
            fail("text graph dimensions are invalid")
        edges = []
        for row in range(edge_count):
            line = source.readline()
            if not line:
                fail(f"text graph is missing edge {row + 1}")
            edges.append(parse_edge(line.split(), row + 1, vertices))
        if any(line.strip() for line in source):
            fail("text graph contains trailing content")
    return vertices, edges


def parse_vna(path):
    vertices = set()
    edges = []
    section = ""
    with open(path, "rt", encoding="utf-8") as source:
        for line in source:
            line = line.strip()
            if not line:
                continue
            if line.lower().startswith("*node"):
                section = "node"
                continue
            if line.lower().startswith("*tie"):
                section = "tie"
                continue
            if line.lower() in ("id", "from to strength"):
                continue
            fields = line.split()
            if section == "node":
                if len(fields) != 1:
                    fail("VNA node row is invalid")
                vertices.add(int(fields[0]))
            elif section == "tie":
                if len(fields) not in (2, 3):
                    fail("VNA tie row is invalid")
                edges.append(parse_edge(fields, len(edges) + 1, 0xffffffff, weighted=True))
            else:
                fail("VNA file has data before a section header")
    if not vertices or min(vertices) < 0 or max(vertices) >= 0xffffffff:
        fail("VNA node IDs are invalid")
    expected = set(range(max(vertices) + 1))
    if vertices != expected:
        fail("VNA node IDs must be contiguous and zero-based")
    checked = []
    for source, target, weight in edges:
        if source not in vertices or target not in vertices:
            fail("VNA tie endpoint is missing from node data")
        checked.append((source, target, weight))
    return max(vertices) + 1, checked


def parse_snap(path):
    vertices = set()
    edges = []
    with gzip.open(path, "rt", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            fields = line.split()
            if not fields or fields[0].startswith("#"):
                continue
            if len(fields) != 2:
                fail(f"SNAP edge {line_number}: expected two fields")
            source_id, target_id = map(int, fields)
            if not (0 <= source_id < 0xffffffff and 0 <= target_id < 0xffffffff):
                fail(f"SNAP edge {line_number}: vertex ID is out of range")
            vertices.add(source_id)
            vertices.add(target_id)
            if source_id == target_id:
                continue
            weight = 1.0
            edges.append((source_id, target_id, weight))
    if not edges:
        fail("SNAP graph contains no edges")
    # SNAP files use sparse original IDs. Preserve their endpoint range as graph IDs.
    return max(vertices) + 1, edges


def _iter_snap_edges(path):
    with gzip.open(path, "rt", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            fields = line.split()
            if not fields or fields[0].startswith("#"):
                continue
            if len(fields) != 2:
                fail(f"SNAP edge {line_number}: expected two fields")
            source_id, target_id = map(int, fields)
            if not (0 <= source_id < 0xffffffff and 0 <= target_id < 0xffffffff):
                fail(f"SNAP edge {line_number}: vertex ID is out of range")
            yield line_number, source_id, target_id


def convert_snap(path, output, expected_vertices=None, expected_edges=None):
    declared_vertices = None
    with gzip.open(path, "rt", encoding="utf-8") as source:
        for line in source:
            fields = line.lstrip("# ").split()
            if "Nodes:" in fields:
                index = fields.index("Nodes:")
                if index + 1 < len(fields):
                    declared_vertices = int(fields[index + 1])
                    break

    max_vertex = -1
    edge_count = 0
    original_ids = set()
    for _, source, target in _iter_snap_edges(path):
        max_vertex = max(max_vertex, source, target)
        original_ids.add(source)
        original_ids.add(target)
        if source != target:
            edge_count += 1
    if max_vertex < 0 or edge_count == 0:
        fail("SNAP graph contains no edges")
    vertices = declared_vertices if declared_vertices is not None else max_vertex + 1
    if declared_vertices is not None and len(original_ids) > declared_vertices:
        fail(f"SNAP endpoint count {len(original_ids)} exceeds declared node count {declared_vertices}")
    compact_ids = None
    if declared_vertices is not None and (len(original_ids) != vertices or max_vertex >= vertices):
        compact_ids = {original: compact for compact, original in enumerate(sorted(original_ids))}
    if expected_vertices is not None and vertices != expected_vertices:
        fail(f"vertex count {vertices} differs from expected {expected_vertices}")
    if expected_edges is not None and edge_count != expected_edges:
        fail(f"edge count {edge_count} differs from expected {expected_edges}")

    output = os.fspath(output)
    temporary = output + ".partial"
    with open(temporary, "wb") as destination:
        destination.write(HEADER.pack(MAGIC, 1, 2, vertices, edge_count))
        written = 0
        for _, source, target in _iter_snap_edges(path):
            if source == target:
                continue
            if compact_ids is not None:
                source, target = compact_ids[source], compact_ids[target]
            destination.write(EDGE.pack(source, target, 1.0))
            written += 1
    if written != edge_count:
        fail("SNAP edge count changed between validation and conversion")
    os.replace(temporary, output)
    print(f"Generated DRGBIN01 input: {output} N={vertices} M={edge_count}")
    return vertices, edge_count


def _matrix_member(archive, path):
    members = [member for member in archive.getmembers()
               if member.isfile() and member.name.lower().endswith(".mtx")]
    if not members:
        fail("archive does not contain a Matrix Market file")
    if len(members) == 1:
        return members[0]
    archive_stem = Path(path).name
    if archive_stem.lower().endswith(".tar.gz"):
        archive_stem = archive_stem[:-7]
    else:
        archive_stem = Path(archive_stem).stem
    preferred = [member for member in members
                 if Path(member.name).stem.lower() == archive_stem.lower()]
    if len(preferred) != 1:
        fail("archive contains multiple Matrix Market files; selection is ambiguous")
    return preferred[0]


def parse_matrix_market(path):
    with tarfile.open(path, "r:*") as archive:
        member = _matrix_member(archive, path)
        stream = archive.extractfile(member)
        if stream is None:
            fail("cannot read Matrix Market file")
        with stream:
            header = stream.readline().decode("ascii", "strict").split()
            if len(header) != 5 or [field.lower() for field in header[:3]] != ["%%matrixmarket", "matrix", "coordinate"]:
                fail("only Matrix Market coordinate format is supported")
            symmetry = header[4].lower()
            while True:
                line = stream.readline()
                if not line:
                    fail("Matrix Market file has no dimensions")
                if not line.startswith(b"%") and line.strip():
                    rows, columns, records = map(int, line.split())
                    break
            if rows <= 0 or rows != columns or records < 0:
                fail("Matrix Market dimensions are invalid")
            edges = []
            for row in range(records):
                line = stream.readline()
                if not line:
                    fail(f"Matrix Market is missing record {row + 1}")
                fields = line.split()
                if not fields or fields[0].startswith(b"%"):
                    fail("Matrix Market record count is invalid")
                source, target = int(fields[0]) - 1, int(fields[1]) - 1
                if not (0 <= source < rows and 0 <= target < rows):
                    fail(f"edge {row + 1}: vertex ID is out of range")
                if source == target:
                    continue
                if len(fields) == 3:
                    matrix_value = float(fields[2])
                    if not math.isfinite(matrix_value):
                        fail(f"edge {row + 1}: matrix value must be finite")
                    if matrix_value == 0.0:
                        continue
                # Matrix Market values describe matrix coefficients. The
                # layout input uses the matrix support as an unweighted graph.
                weight = 1.0
                if symmetry == "general":
                    if source > target:
                        continue
                edges.append((source, target, weight))
    return rows, edges


def convert_matrix_market(path, output, expected_vertices=None, expected_edges=None):
    with tarfile.open(path, "r:*") as archive:
        member = _matrix_member(archive, path)
        stream = archive.extractfile(member)
        if stream is None:
            fail("cannot read Matrix Market file")
        with stream:
            header = stream.readline().decode("ascii", "strict").split()
            if len(header) != 5 or [field.lower() for field in header[:3]] != ["%%matrixmarket", "matrix", "coordinate"]:
                fail("only Matrix Market coordinate format is supported")
            symmetry = header[4].lower()
            while True:
                line = stream.readline()
                if not line:
                    fail("Matrix Market file has no dimensions")
                if not line.startswith(b"%") and line.strip():
                    rows, columns, records = map(int, line.split())
                    break
            if rows <= 0 or rows != columns or records < 0:
                fail("Matrix Market dimensions are invalid")

            output = os.fspath(output)
            temporary = output + ".partial"
            edge_count = 0
            with open(temporary, "wb") as destination:
                destination.write(HEADER.pack(MAGIC, 1, 2, rows, 0))
                for row in range(records):
                    line = stream.readline()
                    if not line:
                        fail(f"Matrix Market is missing record {row + 1}")
                    fields = line.split()
                    if not fields or fields[0].startswith(b"%"):
                        fail("Matrix Market record count is invalid")
                    source, target = int(fields[0]) - 1, int(fields[1]) - 1
                    if not (0 <= source < rows and 0 <= target < rows):
                        fail(f"edge {row + 1}: vertex ID is out of range")
                    if len(fields) == 3:
                        matrix_value = float(fields[2])
                        if not math.isfinite(matrix_value):
                            fail(f"edge {row + 1}: matrix value must be finite")
                        if matrix_value == 0.0:
                            continue
                    if source == target:
                        continue
                    if symmetry == "general":
                        if source > target:
                            continue
                    destination.write(EDGE.pack(source, target, 1.0))
                    edge_count += 1
                if stream.read(1):
                    fail("Matrix Market contains trailing content")
                if expected_vertices is not None and rows != expected_vertices:
                    fail(f"vertex count {rows} differs from expected {expected_vertices}")
                if expected_edges is not None and edge_count != expected_edges:
                    fail(f"edge count {edge_count} differs from expected {expected_edges}")
                destination.seek(0)
                destination.write(HEADER.pack(MAGIC, 1, 2, rows, edge_count))
            os.replace(temporary, output)
    print(f"Generated DRGBIN01 input: {output} N={rows} M={edge_count}")
    return rows, edge_count


def write_graph(path, vertices, edges):
    path = os.fspath(path)
    temporary = path + ".partial"
    with open(temporary, "wb") as destination:
        destination.write(HEADER.pack(MAGIC, 1, 2, vertices, len(edges)))
        for source, target, weight in edges:
            destination.write(EDGE.pack(source, target, weight))
    os.replace(temporary, path)


def main():
    parser = argparse.ArgumentParser(description="Convert a public graph source to DRGBIN01")
    parser.add_argument("format", choices=("text", "vna", "snap", "matrix-market"))
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--vertices", type=int)
    parser.add_argument("--edges", type=int)
    args = parser.parse_args()
    parsers = {"text": parse_text, "vna": parse_vna, "snap": parse_snap,
               "matrix-market": parse_matrix_market}
    if args.format == "matrix-market":
        convert_matrix_market(args.input, args.output, args.vertices, args.edges)
        return
    if args.format == "snap":
        convert_snap(args.input, args.output, args.vertices, args.edges)
        return
    vertices, edges = parsers[args.format](args.input)
    if args.vertices is not None and vertices != args.vertices:
        fail(f"vertex count {vertices} differs from expected {args.vertices}")
    if args.edges is not None and len(edges) != args.edges:
        fail(f"edge count {len(edges)} differs from expected {args.edges}")
    write_graph(args.output, vertices, edges)
    print(f"Generated DRGBIN01 input: {args.output} N={vertices} M={len(edges)}")


if __name__ == "__main__":
    main()
