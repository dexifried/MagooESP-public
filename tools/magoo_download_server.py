"""Serve only the Magoo Android download on loopback for the dedicated tunnel."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse
import json
import zipfile

ROOT=Path(__file__).resolve().parents[1]
class DownloadHandler(BaseHTTPRequestHandler):
 def log_message(self,format,*args): pass
 def do_GET(self): self._serve(False)
 def do_HEAD(self): self._serve(True)
 def _serve(self,head):
  path=self.path.split('?',1)[0]
  try:
   if path=='/api/android/update':
    with zipfile.ZipFile(self.server.apk) as apk:
     info=apk.getinfo('assets/magoo-version.json')
     if info.file_size>1024:raise ValueError('Invalid version metadata')
     version=json.loads(apk.read(info))
    if not isinstance(version,dict):raise ValueError('Invalid version metadata')
    code=version.get('versionCode');name=version.get('versionName')
    if version.get('applicationId')!='ca.magoo.gps' or type(code) is not int or not 0<code<=2100000000 or not isinstance(name,str) or not 0<len(name)<=64:raise ValueError('Invalid version metadata')
    data=json.dumps({'versionCode':code,'versionName':name,'downloadUrl':'https://magoo.dexifyz.com/Magoo.apk'},separators=(',',':')).encode('utf-8')
    mime='application/json; charset=utf-8'
   elif path=='/':data=self.server.landing.read_bytes();mime='text/html; charset=utf-8'
   elif path=='/Magoo.apk':data=self.server.apk.read_bytes();mime='application/vnd.android.package-archive'
   else:self.send_error(404);return
  except (OSError,zipfile.BadZipFile,KeyError,ValueError,TypeError):self.send_error(503,'Download temporarily unavailable');return
  self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
  if path=='/Magoo.apk':self.send_header('Content-Disposition','attachment; filename="Magoo.apk"')
  self.end_headers()
  if not head:self.wfile.write(data)

def create_server(address,port,landing,apk):
 server=ThreadingHTTPServer((address,port),DownloadHandler);server.landing=Path(landing);server.apk=Path(apk);return server

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=8798);parser.add_argument('--apk',type=Path,default=ROOT/'android-app/app/build/outputs/apk/debug/app-debug.apk');args=parser.parse_args()
 create_server('127.0.0.1',args.port,ROOT/'web/download.html',args.apk).serve_forever()
