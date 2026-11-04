"""Exercise layout and settings with a simulated ESP and no real motor output."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
from urllib.parse import parse_qs, urlparse
import sys
import tempfile
import threading

sys.path.insert(0, str(Path(tempfile.gettempdir()) / 'magoo-playwright'))
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
data = {'wifi':'Connected','ip':'192.0.2.1','sleeping':False,'theme':3,'view':'tilt','homeLayout':0,'homeStyle':0,
 'bootAnimation':True,'throttleValid':False,'gearCalibrated':True,'gearDirection':'N','tiltValid':True,'pitch':12.4,'roll':-5.1,
 'batteryValid':False,'batteryVoltage':None,'track':'Song','artist':'Artist','art':0,'position':0,'duration':10,'playing':False,
 'shuffle':False,'trackIndex':0,'gps':False}
calls=[]
tilt_calls=[]
class Server(TCPServer): allow_reuse_address=True
server=Server(('127.0.0.1',0),lambda *a,**kw:SimpleHTTPRequestHandler(*a,directory=str(root/'web'),**kw))
threading.Thread(target=server.serve_forever,daemon=True).start()
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe')
    page=browser.new_page(viewport={'width':393,'height':852},is_mobile=True,has_touch=True)
    page.set_default_timeout(6000);errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    def handle(r):
        path=urlparse(r.request.url).path
        if path=='/status':return r.fulfill(json=data)
        if path=='/networks':return r.fulfill(json={'saved':[],'networks':[],'setupAccess':False,'activeSlot':-1})
        if path=='/winch/status':return r.fulfill(json={'ready':True,'reversed':False,'direction':'S'})
        if path=='/winch':calls.append(parse_qs(r.request.post_data or '').get('direction',[''])[0]);return r.fulfill(status=200,body='OK')
        if path=='/tilt/zero':tilt_calls.append(path);return r.fulfill(status=200,body='OK')
        if path=='/view':data['view']=parse_qs(r.request.post_data or '')['value'][0];return r.fulfill(status=200,body='OK')
        if path=='/playlist':return r.fulfill(json=[])
        if path=='/screen':return r.fulfill(status=204)
        if path in ('/','/dashboard.html','/pairing.css','/pairing.js','/frame-delta.js','/dashboard-polish.css') or path.endswith('.jpg'):return r.continue_()
        return r.fulfill(status=200,body='OK')
    page.route('**/*',handle);page.goto(f'http://127.0.0.1:{server.server_address[1]}/dashboard.html')
    page.locator('#vehicle-live.online').wait_for()
    order=page.evaluate("() => ['display-card','winch-controls','tilt-controls','vehicle-status'].map(id => document.getElementById(id).getBoundingClientRect().top + scrollY)")
    assert order==sorted(order),order
    assert page.locator('#display-card #mirror').count()==1 and page.locator('#display-card #view-home').count()==1
    assert page.locator('#mirror-home').count()==0
    assert page.locator('#vehicle-receiver').count()==0 and page.locator('#remote-connection').count()==1
    assert page.locator('#tilt-controls').is_visible() and page.locator('#tilt-status').inner_text()!='Roll −5.1° · Pitch 12.4°'
    assert page.locator('#winch-reverse').count()==1 and not page.locator('#winch-reverse').is_visible()
    page.locator('#winch-settings summary').click();assert page.locator('#winch-reverse').is_visible()
    b=page.locator('#winch-out');b.scroll_into_view_if_needed();box=b.bounding_box();x=box['x']+box['width']/2;y=box['y']+box['height']/2
    page.mouse.move(x,y);page.mouse.down();page.wait_for_timeout(250);page.mouse.up();page.wait_for_timeout(350)
    assert calls and calls[0]=='out' and calls[-1]=='stop',calls
    page.mouse.move(x,y);page.mouse.down();page.wait_for_timeout(250)
    page.evaluate('openSettings()');page.wait_for_timeout(350)
    assert calls[-1]=='stop',calls
    page.mouse.up();page.locator('#settings-close').click()
    page.locator('#settings-open').click();assert page.locator('#settings-dialog').is_visible()
    assert page.locator('#settings-dialog #themes').count()==1
    assert page.locator('#settings-dialog #home-layout').count()==1
    assert page.locator('#settings-dialog #boot-animation').count()==1
    assert page.locator('#settings-dialog #gear-controls').count()==1
    assert page.locator('#settings-dialog #settings-tilt-zero').count()==1
    page.locator('#settings-tilt-controls summary').click()
    page.locator('#settings-tilt-zero').click()
    page.get_by_text('Level saved.', exact=True).wait_for()
    assert len(tilt_calls)==1
    page.locator('#wifi-open').click();page.get_by_role('heading',name='Get Magoo connected').wait_for();page.locator('#pair-close').click()
    page.locator('#settings-close').click();assert not page.locator('#settings-dialog').is_visible()
    page.locator('#view-nav').click();page.locator('#gps-controls').wait_for(state='visible')
    assert page.locator('#gps-controls a[href="/app.apk"]').count()==1
    page.locator('#view-speed').click();page.locator('#gear-shortcut').wait_for(state='visible')
    page.locator('#gear-shortcut').click();assert page.locator('#settings-dialog').is_visible()
    assert page.locator('#gear-controls').get_attribute('open') is not None
    page.locator('#settings-close').click()
    for width in (320,393,1200):
        page.set_viewport_size({'width':width,'height':852})
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
        assert page.locator('#view-home').bounding_box()['width']>=44
        for control in ('settings-open','power-toggle'):
            box=page.locator('#'+control).bounding_box();assert box['width']>=44 and box['height']>=44
        if width==320:page.screenshot(path=str(Path(tempfile.gettempdir())/'magoo-layout-final.png'))
        for button in ('view-speed','view-nav'):
            assert page.locator('#'+button+' strong').evaluate('el=>el.scrollWidth<=el.clientWidth')
        assert page.locator('#view-speed').get_attribute('aria-label')=='Speedometer'
    assert not errors,errors
    print('PASS: layout priority, contextual controls, settings, pairing, responsive sizing and winch hold/release')
    browser.close()
server.shutdown()
