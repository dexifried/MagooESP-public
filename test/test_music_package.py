"""The optional music package must round-trip the original artwork."""
import sys
import unittest
import base64
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from music_package import build_package, parse_package, source_art


class MusicPackageTest(unittest.TestCase):
    def test_original_art_and_layout_round_trip(self):
        package = build_package(ROOT)
        layout, raw, jpg = parse_package(package)
        original_raw, original_jpg = source_art(ROOT)
        self.assertEqual(raw, original_raw)
        self.assertEqual(jpg, original_jpg)
        self.assertEqual(layout["art"], (10, 27, 60, 60))
        self.assertEqual(layout["title"], (80, 30))

    def test_rejects_corruption_and_truncation(self):
        package = build_package(ROOT)
        for bad in (package[:-1], package[:100] + bytes([package[100] ^ 1]) + package[101:],
                    b"bad!" + package[4:]):
            with self.assertRaises(ValueError):
                parse_package(bad)

    def test_app_bundle_matches_current_source(self):
        script = (ROOT / "web/music-pack.js").read_text(encoding="utf-8")
        match = re.search(r"window\.__magooMusicPack='([^']+)'", script)
        self.assertIsNotNone(match)
        self.assertEqual(base64.b64decode(match.group(1)), build_package(ROOT))


if __name__ == "__main__":
    unittest.main()
