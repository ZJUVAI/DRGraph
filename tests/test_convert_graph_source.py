#!/usr/bin/env python3
import gzip
import importlib.util
import io
import struct
import tarfile
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "convert_graph_source.py"
SPEC = importlib.util.spec_from_file_location("convert_graph_source", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ConverterTests(unittest.TestCase):
    def test_snap_skips_self_loops_and_preserves_sparse_endpoint_range(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "graph.txt.gz"
            with gzip.open(source, "wt", encoding="utf-8") as stream:
                stream.write("# Nodes: 10 Edges: 2\n")
                stream.write("0 0\n")
                stream.write("7 9\n")
            vertices, edges = MODULE.parse_snap(source)
            self.assertEqual(vertices, 10)
            self.assertEqual(edges, [(7, 9, 1.0)])
            output = Path(directory) / "graph.binary"
            MODULE.convert_snap(source, output, expected_vertices=10, expected_edges=1)
            data = output.read_bytes()
            self.assertEqual(len(data), 44)
            self.assertEqual(struct.unpack("<IIf", data[32:]), (1, 2, 1.0))

    def test_snap_compacts_sparse_ids_when_endpoint_count_matches_header(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "graph.txt.gz"
            with gzip.open(source, "wt", encoding="utf-8") as stream:
                stream.write("# Nodes: 2 Edges: 1\n")
                stream.write("100 200\n")
            output = Path(directory) / "graph.binary"
            MODULE.convert_snap(source, output, expected_vertices=2, expected_edges=1)
            self.assertEqual(struct.unpack("<IIf", output.read_bytes()[32:]), (0, 1, 1.0))

    def test_matrix_market_pattern_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "matrix.tar.gz"
            payload = (b"%%MatrixMarket matrix coordinate pattern symmetric\n"
                       b"% comment\n3 3 1\n1 2\n")
            with tarfile.open(source, "w:gz") as archive:
                info = tarfile.TarInfo("sample/sample.mtx")
                info.size = len(payload)
                archive.addfile(info, io.BytesIO(payload))
            vertices, edges = MODULE.parse_matrix_market(source)
            self.assertEqual(vertices, 3)
            self.assertEqual(edges, [(0, 1, 1.0)])

    def test_matrix_market_general_archive_deduplicates_reverse_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "venkat50.tar.gz"
            payload = (b"%%MatrixMarket matrix coordinate real general\n"
                       b"3 3 4\n1 1 2.0\n1 2 -3.0\n2 1 3.0\n3 2 0.0\n")
            with tarfile.open(source, "w:gz") as archive:
                info = tarfile.TarInfo("venkat50/venkat50.mtx")
                info.size = len(payload)
                archive.addfile(info, io.BytesIO(payload))
                other = tarfile.TarInfo("venkat50/venkat50_b.mtx")
                other_payload = b"not selected\n"
                other.size = len(other_payload)
                archive.addfile(other, io.BytesIO(other_payload))
            vertices, edges = MODULE.parse_matrix_market(source)
            self.assertEqual(vertices, 3)
            self.assertEqual(edges, [(0, 1, 1.0)])

    def test_write_graph_has_exact_binary_shape(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "sample.graph"
            MODULE.write_graph(output, 3, [(0, 2, 1.5)])
            data = output.read_bytes()
            self.assertEqual(len(data), 32 + 12)
            self.assertEqual(struct.unpack("<8sIIQQ", data[:32]),
                             (b"DRGBIN01", 1, 2, 3, 1))
            self.assertEqual(struct.unpack("<IIf", data[32:]), (0, 2, 1.5))


if __name__ == "__main__":
    unittest.main()
