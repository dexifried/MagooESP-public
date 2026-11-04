"""Tests for the local, narrowly scoped Magoo download server."""
import http.client
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest
import json
import zipfile

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('magoo_download_server',root/'tools/magoo_download_server.py')
server_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(server_module)

class DownloadServerTest(unittest.TestCase):
 def test_update_metadata_matches_the_served_apk(self):
  with tempfile.TemporaryDirectory() as d:
   apk=Path(d)/'app.apk'
   with zipfile.ZipFile(apk,'w') as package:
    package.writestr('assets/magoo-version.json',json.dumps({'applicationId':'ca.magoo.gps','versionCode':3,'versionName':'3.0-test'}))
   server=server_module.create_server('127.0.0.1',0,root/'web/download.html',apk)
   thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
   try:
    c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=2)
    c.request('GET','/api/android/update');r=c.getresponse();self.assertEqual(r.status,200)
    self.assertEqual(r.getheader('Cache-Control'),'no-store')
    data=json.loads(r.read());self.assertEqual(data['versionCode'],3);self.assertEqual(data['versionName'],'3.0-test')
    self.assertEqual(data['downloadUrl'],'https://magoo.dexifyz.com/Magoo.apk')
    c.request('HEAD','/api/android/update');r=c.getresponse();self.assertEqual(r.status,200);self.assertEqual(r.read(),b'')
    with zipfile.ZipFile(apk,'w') as package: package.writestr('unrelated','no version')
    c.request('GET','/api/android/update');r=c.getresponse();self.assertEqual(r.status,503);r.read();c.close()
   finally:server.shutdown();server.server_close();thread.join()
 def test_only_landing_page_and_apk_are_served(self):
  with tempfile.TemporaryDirectory() as d:
   apk=Path(d)/'app.apk';apk.write_bytes(b'test-apk')
   server=server_module.create_server('127.0.0.1',0,root/'web/download.html',apk)
   thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
   try:
    c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=2)
    c.request('GET','/');r=c.getresponse();self.assertEqual(r.status,200);self.assertIn(b'Magoo',r.read())
    c.request('GET','/Magoo.apk');r=c.getresponse();self.assertEqual(r.status,200);self.assertEqual(r.getheader('Content-Type'),'application/vnd.android.package-archive');self.assertEqual(r.read(),b'test-apk')
    c.request('GET','/../app.apk');r=c.getresponse();self.assertEqual(r.status,404);r.read();c.close()
   finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':unittest.main()
