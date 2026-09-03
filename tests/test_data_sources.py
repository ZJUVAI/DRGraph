#!/usr/bin/env python3
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DataSourceTests(unittest.TestCase):
    def test_data_sources_do_not_follow_master_branches(self):
        source_text = "\n".join(
            (ROOT / path).read_text(encoding="utf-8")
            for path in ("data/datasets.yml", "scripts/prepare.sh")
        )
        self.assertIsNone(
            re.search(r"(?:/master/|/refs/heads/master/)", source_text),
            "data sources must use local files or pinned remote revisions",
        )


if __name__ == "__main__":
    unittest.main()
