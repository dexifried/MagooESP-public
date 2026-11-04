"""Exercise the standby page without sending commands to hardware."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
import sys,tempfile,threading
sys.path.insert(0,str(Path(tempfile.gettempdir())/'magoo-playwright'))
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]
class Server(TCPServer): allow_reuse_address=True
server=Server(('127.0.0.1',0),lambda *a,**kw:SimpleHTTPRequestHandler(*a,directory=str(root/'web'),**kw))
threading.Thread(target=server.serve_forever,daemon=True).start()
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe')
 page=browser.new_page(viewport={'width':393,'height':852},is_mobile=True,has_touch=True)
 def route(r):
  url=r.request.url
  if url.endswith('/status'):return r.fulfill(json={'sleeping':False,'webUnlocked':False})
  if url.endswith('/sleep'):
   assert 'web=1' in (r.request.post_data or '')
   return r.fulfill(status=200,body='OK')
  if url.endswith('/') and r.request.method=='GET':return r.fulfill(status=200,body='Awake dashboard')
  return r.continue_()
 page.route('**/*',route)
 page.goto(f'http://127.0.0.1:{server.server_address[1]}/standby.html')
 assert page.get_by_role('heading',name='Currently Asleep').is_visible()
 assert page.get_by_role('link',name='Use app').get_attribute('href')=='https://magoo.dexifyz.com/'
 page.wait_for_timeout(5200)
 assert page.get_by_role('heading',name='Currently Asleep').is_visible()
 page.get_by_role('button',name='Wake Up').click()
 page.get_by_text('Awake dashboard').wait_for()
 print('PASS: standby screen, app link and wake flow')
 browser.close()
server.shutdown()
