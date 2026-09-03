#!/usr/bin/env python3
import importlib.util
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "visualize.py"
SPEC = importlib.util.spec_from_file_location("visualize", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class VisualizeTests(unittest.TestCase):
    def test_edge_sampling_ratio_scales_large_graphs(self):
        self.assertEqual(MODULE.edge_sampling_ratio(599999), 1.0)
        self.assertEqual(MODULE.edge_sampling_ratio(600000), 0.1)
        self.assertEqual(MODULE.edge_sampling_ratio(2999999), 0.1)
        self.assertEqual(MODULE.edge_sampling_ratio(3000000), 0.001)

    def test_sampled_edge_limit_applies_ratio_and_cli_cap(self):
        self.assertEqual(MODULE.sampled_edge_limit(117185083, 300000), 117186)
        self.assertEqual(MODULE.sampled_edge_limit(117185083, 1000), 1000)
        self.assertEqual(MODULE.sampled_edge_limit(100, 300000), 100)
        self.assertEqual(MODULE.sampled_edge_limit(100, 0), 0)


if __name__ == "__main__":
    unittest.main()
