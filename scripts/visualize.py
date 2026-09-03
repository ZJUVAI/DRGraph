#!/usr/bin/env python3
"""Render a DRGBIN01 input and its text embedding as a PNG."""

import argparse
import math
import os
import struct
import sys
from typing import Optional, Tuple

import matplotlib

matplotlib.use("Agg")
from matplotlib import pyplot as plt
from matplotlib.collections import LineCollection
import numpy as np


MAGIC = b"DRGBIN01"
VERSION = 1
DATA_KIND = 1
GRAPH_KIND = 2
HEADER = struct.Struct("<8sIIQQ")
EDGE_DTYPE = np.dtype([("source", "<u4"), ("target", "<u4"), ("weight", "<f4")])
HEADER_BYTES = HEADER.size


def fail(message: str) -> "None":
    raise ValueError(message)


def read_header(path: str, expected_kind: int) -> Tuple[int, int, int]:
    file_size = os.path.getsize(path)
    if file_size < HEADER_BYTES:
        fail(f"{path}: binary header is incomplete")
    with open(path, "rb") as stream:
        raw = stream.read(HEADER_BYTES)
    magic, version, kind, first_count, second_count = HEADER.unpack(raw)
    if magic != MAGIC:
        fail(f"{path}: unsupported binary magic")
    if version != VERSION:
        fail(f"{path}: unsupported binary version {version}")
    if kind != expected_kind:
        fail(f"{path}: binary kind does not match the file extension")
    return file_size, first_count, second_count


def load_data(path: str) -> Tuple[int, int, np.ndarray]:
    file_size, count, dimension = read_header(path, DATA_KIND)
    expected_size = HEADER_BYTES + count * dimension * 4
    if count == 0 or dimension == 0:
        fail(f"{path}: .data requires positive N and D")
    if expected_size != file_size:
        fail(f"{path}: length does not match the header counts")
    with open(path, "rb") as stream:
        stream.seek(HEADER_BYTES)
        values = np.fromfile(stream, dtype="<f4", count=count * dimension)
    if values.size != count * dimension or not np.isfinite(values).all():
        fail(f"{path}: vector payload is incomplete or contains non-finite values")
    return count, dimension, values.reshape((count, dimension))


def edge_sampling_ratio(edge_count: int) -> float:
    """Return the drawing ratio for a graph's total edge count."""
    if edge_count < 600000:
        return 1.0
    if edge_count < 3000000:
        return 0.1
    return 0.001


def sampled_edge_limit(edge_count: int, max_edges: int) -> int:
    """Bound graph rendering by both the dataset ratio and the CLI limit."""
    if max_edges == 0:
        return 0
    ratio_limit = max(1, math.ceil(edge_count * edge_sampling_ratio(edge_count)))
    return min(edge_count, max_edges, ratio_limit)


def load_graph(path: str, max_edges: int, seed: int) -> Tuple[int, int, np.ndarray]:
    file_size, count, edge_count = read_header(path, GRAPH_KIND)
    expected_size = HEADER_BYTES + edge_count * EDGE_DTYPE.itemsize
    if count == 0:
        fail(f"{path}: .graph requires positive N")
    if expected_size != file_size:
        fail(f"{path}: length does not match the header counts")

    sample_limit = sampled_edge_limit(edge_count, max_edges)
    selected = np.empty(sample_limit, dtype=EDGE_DTYPE)
    selected_positions = None
    if max_edges > 0:
        generator = np.random.default_rng(seed)
        selected_positions = np.sort(generator.choice(edge_count, size=sample_limit, replace=False))
    seen = 0
    chunk_records = max(1, (8 * 1024 * 1024) // EDGE_DTYPE.itemsize)
    with open(path, "rb") as stream:
        stream.seek(HEADER_BYTES)
        while seen < edge_count:
            chunk_size = min(chunk_records, edge_count - seen)
            chunk = np.fromfile(stream, dtype=EDGE_DTYPE, count=chunk_size)
            if chunk.size != chunk_size:
                fail(f"{path}: edge payload is incomplete")
            if (chunk["source"] >= count).any() or (chunk["target"] >= count).any():
                fail(f"{path}: edge endpoint is out of range")
            if (chunk["source"] == chunk["target"]).any():
                fail(f"{path}: self-loops are not supported")
            if (not np.isfinite(chunk["weight"]).all() or (chunk["weight"] <= 0).any()):
                fail(f"{path}: edge weights must be finite and positive")

            if max_edges > 0:
                begin = int(np.searchsorted(selected_positions, seen, side="left"))
                end = int(np.searchsorted(selected_positions, seen + chunk_size, side="left"))
                if end > begin:
                    local_positions = selected_positions[begin:end] - seen
                    selected[begin:end] = chunk[local_positions]
            seen += chunk_size

    if max_edges > 0 and edge_count <= max_edges:
        return count, edge_count, selected[:edge_count]
    if max_edges == 0:
        return count, edge_count, selected[:0]
    return count, edge_count, selected


def load_embedding(path: str, expected_count: int) -> np.ndarray:
    try:
        with open(path, "rt", encoding="utf-8") as stream:
            first = stream.readline()
            if not first:
                fail(f"{path}: embedding is empty")
            fields = first.split()
            if len(fields) != 2 or any(not item.isdecimal() for item in fields):
                fail(f"{path}: first line must contain N and output dimension")
            count, dimension = (int(item) for item in fields)
            if count != expected_count or dimension == 0:
                fail(f"{path}: embedding shape does not match the input")
            rows = []
            for row in range(count):
                line = stream.readline()
                if not line:
                    fail(f"{path}: missing embedding row {row + 1}")
                values = line.split()
                if len(values) != dimension:
                    fail(f"{path}: invalid dimension in row {row + 1}")
                try:
                    parsed = [float(value) for value in values]
                except ValueError:
                    fail(f"{path}: invalid number in row {row + 1}")
                if not np.isfinite(parsed).all():
                    fail(f"{path}: embedding contains non-finite values")
                rows.append(parsed)
            if any(line.strip() for line in stream):
                fail(f"{path}: embedding contains trailing content")
    except OSError as error:
        fail(f"Cannot read embedding {path}: {error}")
    return np.asarray(rows, dtype=np.float64)


def load_labels(path: str, expected_count: int) -> np.ndarray:
    labels = []
    try:
        with open(path, "rt", encoding="utf-8") as stream:
            for row, line in enumerate(stream, 1):
                fields = line.split()
                if len(fields) != 1:
                    fail(f"{path}: row {row} must contain one non-negative integer")
                try:
                    value = int(fields[0])
                except ValueError:
                    fail(f"{path}: row {row} must contain one non-negative integer")
                if value < 0:
                    fail(f"{path}: row {row} must contain one non-negative integer")
                labels.append(value)
    except OSError as error:
        fail(f"Cannot read labels {path}: {error}")
    if len(labels) != expected_count:
        fail(f"{path}: expected {expected_count} labels, found {len(labels)}")
    return np.asarray(labels)


def set_square_limits(axis, coordinates: np.ndarray) -> None:
    x_center = float((coordinates[:, 0].min() + coordinates[:, 0].max()) * 0.5)
    y_center = float((coordinates[:, 1].min() + coordinates[:, 1].max()) * 0.5)
    span = max(
        float(coordinates[:, 0].max() - coordinates[:, 0].min()),
        float(coordinates[:, 1].max() - coordinates[:, 1].min()),
    )
    if span == 0.0:
        span = max(1.0, abs(x_center), abs(y_center))
    half_span = span * 0.5 * 1.04
    axis.set_xlim(x_center - half_span, x_center + half_span)
    axis.set_ylim(y_center - half_span, y_center + half_span)


def plot_embedding(embedding: np.ndarray, labels: Optional[np.ndarray], output: str) -> None:
    if embedding.shape[1] == 1:
        coordinates = np.column_stack((embedding[:, 0], np.zeros(embedding.shape[0])))
    else:
        coordinates = embedding[:, :2]
    figure, axis = plt.subplots(figsize=(10, 10), constrained_layout=True)
    scatter = axis.scatter(
        coordinates[:, 0],
        coordinates[:, 1],
        c=labels if labels is not None else "#2563eb",
        cmap="tab20" if labels is not None else None,
        s=4,
        alpha=0.85,
        linewidths=0,
    )
    if labels is not None and len(np.unique(labels)) <= 20:
        figure.colorbar(scatter, ax=axis, fraction=0.046, pad=0.04, label="Label")
    set_square_limits(axis, coordinates)
    axis.set_aspect("equal", adjustable="datalim")
    axis.set_axis_off()
    figure.savefig(output, dpi=300, bbox_inches="tight", pad_inches=0.02)
    plt.close(figure)


def plot_graph(embedding: np.ndarray, edges: np.ndarray, total_edges: int, output: str) -> None:
    if embedding.shape[1] == 1:
        coordinates = np.column_stack((embedding[:, 0], np.zeros(embedding.shape[0])))
    else:
        coordinates = embedding[:, :2]
    figure, axis = plt.subplots(figsize=(10, 10), constrained_layout=True)
    if edges.size:
        segments = coordinates[edges["source"].astype(np.intp)][:, None, :]
        segments = np.concatenate((segments, coordinates[edges["target"].astype(np.intp)][:, None, :]), axis=1)
        lengths = np.linalg.norm(segments[:, 0] - segments[:, 1], axis=1)
        if lengths.max() > lengths.min():
            lengths = (lengths - lengths.min()) / (lengths.max() - lengths.min())
        line_width = 0.02 if total_edges >= 400000 else 0.12 if total_edges >= 5000 else 0.4
        collection = LineCollection(
            segments, array=lengths, cmap="jet_r", linewidths=line_width,
            alpha=0.8, capstyle="round", joinstyle="round",
        )
        axis.add_collection(collection)
    set_square_limits(axis, coordinates)
    axis.set_aspect("equal", adjustable="box")
    axis.set_axis_off()
    figure.savefig(output, dpi=300, bbox_inches="tight", pad_inches=0.02)
    plt.close(figure)


def main() -> int:
    parser = argparse.ArgumentParser(description="Visualize a DRGBIN01 input and its embedding")
    parser.add_argument("--input", required=True, help="Binary .data or .graph input")
    parser.add_argument("--embedding", required=True, help="Text embedding written by drgraph")
    parser.add_argument("--labels", help="Optional one-label-per-line file")
    parser.add_argument("--output", required=True, help="Output PNG path")
    parser.add_argument(
        "--max-edges", type=int, default=300000,
        help="Maximum graph edges to draw; large graphs also use a sparse ratio",
    )
    parser.add_argument("--seed", type=int, default=42, help="Seed for graph edge sampling")
    args = parser.parse_args()
    if args.max_edges < 0:
        parser.error("--max-edges must be non-negative")
    try:
        extension = os.path.splitext(args.input)[1]
        if extension == ".data":
            count, _, _ = load_data(args.input)
            embedding = load_embedding(args.embedding, count)
            labels = load_labels(args.labels, count) if args.labels else None
            plot_embedding(embedding, labels, args.output)
        elif extension == ".graph":
            count, total_edges, edges = load_graph(args.input, args.max_edges, args.seed)
            embedding = load_embedding(args.embedding, count)
            labels = load_labels(args.labels, count) if args.labels else None
            plot_graph(embedding, edges, total_edges, args.output)
        else:
            fail("Input extension must be .data or .graph")
    except (OSError, ValueError, struct.error) as error:
        print(f"Visualization failed: {error}", file=sys.stderr)
        return 2
    print(f"Visualization complete: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
