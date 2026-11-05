"""Exercise the Expo dashboard against a simulated ESP; no real motor output."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
from urllib.parse import parse_qs,urlparse
import sys,tempfile,threading,time
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'));from build_expo_dashboard import build_dashboard
sys.path.insert(0,str(Path(tempfile.gettempdir())/'magoo-playwright'))
from playwright.sync_api import sync_playwright
class Server(TCPServer):allow_reuse_address=True
server=Server(('127.0.0.1',0),lambda *a,**kw:SimpleHTTPRequestHandler(*a,directory=str(root/'web'),**kw));threading.Thread(target=server.serve_forever,daemon=True).start()
data={'wifi':'Connected','ip':'192.0.2.1','sleeping':False,'theme':3,'view':'navigation','homeLayout':0,'homeStyle':0,'bootAnimation':True,'throttleValid':False,'gearCalibrated':True,'gearDirection':'N','tiltValid':True,'pitch':12.4,'roll':-5.1,'batteryValid':False,'batteryVoltage':None,'track':'Song','artist':'Artist','art':0,'position':0,'duration':10,'playing':False,'shuffle':False,'trackIndex':0,'gps':False}
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe');page=browser.new_page(viewport={'width':393,'height':852},is_mobile=True,has_touch=True)
 calls=[];maps=[];refreshes=[];other=[];errors=[];screens=[];deltas=[];statuses=[];delta_times=[];map_state={'refresh_status':200};page.on('pageerror',lambda e:errors.append(str(e)))
 page.add_init_script('''window.nativeMessages=[];const nativeFetch=window.fetch.bind(window);window.ReactNativeWebView={postMessage:s=>{const m=JSON.parse(s);window.nativeMessages.push(m);if(m.type==='tile'){const c=document.createElement('canvas');c.width=256;c.height=256;c.getContext('2d').fillRect(0,0,256,256);setTimeout(()=>window.__magooTileResult({id:m.id,data:c.toDataURL()}),0)}if(m.type==='http-request'){nativeFetch(m.path,{method:m.method,headers:m.headers,body:m.body}).then(async r=>{const result={id:m.id,status:r.status,headers:{'x-display-state':r.headers.get('x-display-state')}};if((m.path==='/screen'||m.path==='/screen-delta')&&r.status===200){const b=new Uint8Array(await r.arrayBuffer());result.base64=btoa(String.fromCharCode(...b))}else result.text=await r.text();window.__magooHttpResult(result)}).catch(e=>window.__magooHttpResult({id:m.id,error:e.message}))}}};''')
 def handle(r):
  path=urlparse(r.request.url).path
  if path=='/':return r.fulfill(status=200,content_type='text/html',body=build_dashboard(root))
  if path=='/status':statuses.append(1);return r.fulfill(json=data)
  if path=='/winch/status':return r.fulfill(json={'ready':True,'reversed':False,'direction':'S'})
  if path=='/screen':screens.append(path);return r.fulfill(status=200,body=bytes([0,248])*(240*135))
  if path=='/screen-delta':
   deltas.append(parse_qs(r.request.post_data or ''))
   delta_times.append(time.monotonic())
   payload=bytes([77,68,76,49,1,0,0])+bytes(480) if len(deltas)==1 else bytes([77,68,76,49,0,0])
   return r.fulfill(status=200,body=payload)
  if path=='/playlist':return r.fulfill(json=[])
  if path=='/winch':calls.append(parse_qs(r.request.post_data or '').get('direction',[''])[0]);return r.fulfill(status=200,body='OK')
  if path=='/map':maps.append(parse_qs(r.request.post_data or ''));return r.fulfill(status=200,body='OK:123')
  if path=='/map/refresh':refreshes.append(parse_qs(r.request.post_data or ''));return r.fulfill(status=map_state['refresh_status'],body='OK')
  other.append(path);return r.fulfill(status=200,body='OK')
 page.route('**/*',handle);page.goto('http://127.0.0.1:'+str(server.server_address[1])+'/')
 page.locator('#conn.online').wait_for();page.locator('#gps-controls').wait_for(state='visible')
 page.wait_for_function("document.getElementById('display-badge').textContent==='LIVE'")
 page.wait_for_timeout(2400)
 assert len(screens)==1 and len(deltas)>=2 and len(deltas[0]['hashes'][0])==135*8,(screens,deltas)
 assert delta_times[1]-delta_times[0]<1.1,delta_times
 page.evaluate('window.__magooSetAppActive(false)')
 page.wait_for_timeout(200);before=(len(screens),len(deltas),len(statuses))
 page.wait_for_timeout(1800)
 assert (len(screens),len(deltas),len(statuses))==before,'background should not poll'
 page.evaluate('window.__magooSetAppActive(true)')
 page.wait_for_timeout(400)
 assert len(deltas)>before[1] and len(statuses)>before[2],'foreground should refresh immediately'
 assert page.locator('#display-card .app-icon').count()==5
 assert not [p for p in other if p.endswith(('.jpg','.css','.js'))],other
 page.locator('#gps-controls a[href="/app.apk"]').click()
 assert page.evaluate("nativeMessages.some(m=>m.type==='gps-controls')")
 assert page.evaluate("async()=>{const c=new AbortController();const p=fetch('/status',{signal:c.signal});c.abort();try{await p;return false}catch(e){return e.name==='AbortError'}}")
 assert page.evaluate("nativeMessages.some(m=>m.type==='http-cancel')")
 page.evaluate("window.__magooApplyLocation({latitude:43.65,longitude:-79.38,heading:90,speed:1.2,timestamp:Date.now()})")
 page.wait_for_function("nativeMessages.some(m=>m.type==='gps-status' && m.message.includes('Sharing'))",timeout=10000)
 assert len(maps)==1 and len(maps[0]['pixels'][0])==72*52*2 and maps[0]['bearing']==['90']
 page.evaluate("window.__fakeNow=Date.now()+11000;Date.now=()=>window.__fakeNow")
 page.evaluate("window.__magooApplyLocation({latitude:43.65,longitude:-79.38,heading:91,speed:1.2,timestamp:Date.now()})")
 page.wait_for_function("nativeMessages.filter(m=>m.type==='gps-status' && m.message.includes('Sharing')).length>=2",timeout=10000)
 assert len(maps)==1 and len(refreshes)==1 and refreshes[0]['token']==['123'] and refreshes[0]['bearing']==['91']
 map_state['refresh_status']=409
 page.evaluate("window.__fakeNow+=11000;window.__magooApplyLocation({latitude:43.65,longitude:-79.38,heading:91,speed:1.2,timestamp:Date.now()})")
 page.wait_for_function("nativeMessages.filter(m=>m.type==='gps-status' && m.message.includes('Sharing')).length>=3",timeout=10000)
 assert len(maps)==2 and len(refreshes)==2
 assert page.evaluate("nativeMessages.some(m=>m.type==='gps-demand' && m.value===true)")
 page.evaluate("currentView='music';updateViews()")
 assert page.evaluate("nativeMessages.filter(m=>m.type==='gps-demand').slice(-1)[0].value===false")
 page.evaluate("currentView='navigation';updateViews()")
 assert page.evaluate("nativeMessages.filter(m=>m.type==='gps-demand').slice(-1)[0].value===true")
 page.evaluate("setPowerState(true)")
 assert page.evaluate("nativeMessages.filter(m=>m.type==='gps-demand').slice(-1)[0].value===false")
 page.evaluate("setPowerState(false)")
 assert page.evaluate("nativeMessages.filter(m=>m.type==='gps-demand').slice(-1)[0].value===true")
 page.locator('#mirror').scroll_into_view_if_needed()
 page.wait_for_function("document.getElementById('display-badge').textContent==='LIVE'",timeout=5000)
 assert len(screens)>=2, 'waking must reload the cleared mirror'
 b=page.locator('#winch-out');b.scroll_into_view_if_needed();box=b.bounding_box();page.mouse.move(box['x']+10,box['y']+10);page.mouse.down();page.wait_for_timeout(250);page.mouse.up();page.wait_for_timeout(350)
 assert calls and calls[0]=='out' and calls[-1]=='stop',calls
 assert not errors,errors
 print('PASS: bundled icons, app controls, map conversion and winch hold/release')
 page.close()
 legacy=browser.new_page(viewport={'width':393,'height':852})
 legacy_frames=[];legacy_deltas=[];legacy_errors=[]
 legacy.on('pageerror',lambda e:legacy_errors.append(str(e)))
 def legacy_handle(r):
  path=urlparse(r.request.url).path
  if path=='/status':return r.fulfill(json=data)
  if path=='/screen':legacy_frames.append(1);return r.fulfill(status=200,body=bytes([0,248])*(240*135))
  if path=='/screen-delta':legacy_deltas.append(1);return r.fulfill(status=404,body='Not found')
  if path=='/winch/status':return r.fulfill(json={'ready':True,'reversed':False,'direction':'S'})
  if path=='/playlist':return r.fulfill(json=[])
  if path in ('/dashboard.html','/pairing.css','/dashboard-polish.css','/pairing.js','/frame-delta.js','/music-installer.js','/music-pack.js') or path.endswith('.jpg'):return r.continue_()
  return r.fulfill(status=200,body='OK')
 legacy.route('**/*',legacy_handle)
 legacy.goto('http://127.0.0.1:'+str(server.server_address[1])+'/dashboard.html?app=1')
 legacy.wait_for_timeout(3500)
 assert len(legacy_frames)>=2 and len(legacy_deltas)==1,(legacy_frames,legacy_deltas)
 assert legacy.locator('#display-badge').inner_text()=='LIVE' and not legacy_errors,legacy_errors
 print('PASS: older firmware falls back to the original screen mirror')
 browser.close()
server.shutdown()
