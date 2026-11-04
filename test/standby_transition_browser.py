"""The browser transitions to standby without affecting app-mode clients."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
from urllib.parse import urlparse
import sys,tempfile,threading
sys.path.insert(0,str(Path(tempfile.gettempdir())/'magoo-playwright'))
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]
class Server(TCPServer): allow_reuse_address=True
server=Server(('127.0.0.1',0),lambda *a,**kw:SimpleHTTPRequestHandler(*a,directory=str(root/'web'),**kw));threading.Thread(target=server.serve_forever,daemon=True).start()
data={'wifi':'Connected','ip':'192.0.2.1','sleeping':True,'theme':3,'view':'tilt','homeLayout':0,'homeStyle':0,'bootAnimation':True,'throttleValid':False,'gearCalibrated':True,'gearDirection':'N','tiltValid':True,'pitch':12.4,'roll':-5.1,'batteryValid':False,'batteryVoltage':None,'track':'Song','artist':'Artist','art':0,'position':0,'duration':10,'playing':False,'shuffle':False,'trackIndex':0,'gps':False}
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe');page=browser.new_page()
 def route(r):
  path=urlparse(r.request.url).path
  if path=='/':return r.fulfill(status=200,content_type='text/html',body=(root/'web/standby.html').read_text(encoding='utf-8-sig'))
  if path=='/status':return r.fulfill(json=data)
  if path=='/winch/status':return r.fulfill(json={'ready':True,'reversed':False,'direction':'S'})
  if path=='/screen':return r.fulfill(status=204,headers={'X-Display-State':'sleep'})
  if path=='/playlist':return r.fulfill(json=[])
  if path in ('/dashboard.html','/pairing.css','/pairing.js','/frame-delta.js','/dashboard-polish.css') or path.endswith('.jpg'):return r.continue_()
  return r.fulfill(status=200,body='OK')
 page.route('**/*',route);page.goto(f'http://127.0.0.1:{server.server_address[1]}/dashboard.html')
 page.get_by_role('heading',name='Hey!').wait_for();assert page.locator('#winch-controls').count()==0
 print('PASS: web dashboard switches to standby when the display sleeps')
 browser.close()
server.shutdown()
