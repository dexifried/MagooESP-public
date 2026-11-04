from pathlib import Path
import gzip
import re
import unittest
import sys


class EmbeddedDashboardTest(unittest.TestCase):
    def test_compressed_dashboard_matches_phone_page(self):
        root = Path(__file__).resolve().parents[1]
        header = (root / 'include/DashboardPage.h').read_text(encoding='utf-8')
        match = re.search(r'DASHBOARD_PAGE_GZIP\[\].*?\{(.*?)\}', header, re.S)
        self.assertIsNotNone(match, 'compressed dashboard missing from generated header')
        compressed = bytes(map(int, re.findall(r'\d+', match.group(1))))
        sys.path.insert(0, str(root / 'tools'))
        from embed_dashboard import render_page
        page = render_page(root)
        self.assertEqual(gzip.decompress(compressed).decode('utf-8'), page)


if __name__ == '__main__':
    unittest.main()
