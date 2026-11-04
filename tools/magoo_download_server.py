"""Serve only the Magoo Android download on loopback for the dedicated tunnel."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse

ROOT=Path(__file__).resolve().parents[1]
class DownloadHandler(BaseHTTPRequestHandler):
 def log_message(self,format,*args): pass
 def do_GET(self): self._serve(False)
 def do_HEAD(self): self._serve(True)
 def _serve(self,head):
  path=self.path.split('?',1)[0]
  if path=='/':target=self.server.landing;mime='text/html; charset=utf-8'
  elif path=='/Magoo.apk':target=self.server.apk;mime='application/vnd.android.package-archive'
  else:self.send_error(404);return
  try:data=target.read_bytes()
  except OSError:self.send_error(503,'Download temporarily unavailable');return
  self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
  if path=='/Magoo.apk':self.send_header('Content-Disposition','attachment; filename="Magoo.apk"')
  self.end_headers()
  if not head:self.wfile.write(data)

def create_server(address,port,landing,apk):
 server=ThreadingHTTPServer((address,port),DownloadHandler);server.landing=Path(landing);server.apk=Path(apk);return server

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=8798);parser.add_argument('--apk',type=Path,default=ROOT/'android-app/app/build/outputs/apk/debug/app-debug.apk');args=parser.parse_args()
 create_server('127.0.0.1',args.port,ROOT/'web/download.html',args.apk).serve_forever()
