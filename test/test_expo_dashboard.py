"""The Expo copy must carry the real dashboard and its artwork locally."""
from pathlib import Path
import re,unittest
root=Path(__file__).resolve().parents[1]
class ExpoDashboardTest(unittest.TestCase):
 def test_bundle_has_local_assets_and_no_web_sleep_redirect(self):
  import sys
  sys.path.insert(0,str(root/'tools'))
  from build_expo_dashboard import build_dashboard
  page=build_dashboard(root)
  self.assertIn('const nativeApp=true;',page)
  self.assertIn('data:image/jpeg;base64,',page)
  self.assertNotIn("url('/hero.jpg')",page)
  self.assertIn('window.__magooAlbum',page)
  self.assertIn('window.__magooApplyLocation',page)
  for name in ('home','speed','nav','music','tilt'):
   self.assertRegex(page,r'<span class="app-icon '+name+r'"')
if __name__=='__main__':unittest.main()
