"""Exercise dashboard feedback without sending commands to real hardware."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
import sys
import tempfile
import threading
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, str(Path(tempfile.gettempdir()) / 'magoo-playwright'))
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
state = {'winch': [], 'tilt_ok': True, 'online': True, 'winch_ok': True}
telemetry = {'wifi':'Connected','ip':'192.0.2.1','sleeping':False,'theme':3,'view':'tilt','homeLayout':0,'homeStyle':0,
 'bootAnimation':True,'throttleValid':True,'gearCalibrated':True,'gearDirection':'D','tiltValid':True,'pitch':12.4,'roll':-5.1,
 'batteryValid':False,'batteryVoltage':None,'track':'Song','artist':'Artist','art':0,'position':0,'duration':10,'playing':False,
 'shuffle':False,'trackIndex':0,'gps':False}
class Server(TCPServer): allow_reuse_address = True
server = Server(('127.0.0.1',0),lambda *a,**kw: SimpleHTTPRequestHandler(*a,directory=str(root/'web'),**kw))
threading.Thread(target=server.serve_forever,daemon=True).start()
with sync_playwright() as playwright:
    browser=playwright.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe')
    page=browser.new_page(viewport={'width':393,'height':852},is_mobile=True,has_touch=True)
    page.set_default_timeout(5000)
    errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    def route(r):
        path=urlparse(r.request.url).path
        if path=='/status':return r.fulfill(json=telemetry) if state['online'] else r.fulfill(status=503,body='offline')
        if path=='/networks':return r.fulfill(json={'saved':[],'networks':[],'setupAccess':False,'activeSlot':-1})
        if path=='/winch/status':return r.fulfill(json={'ready':True,'reversed':False,'direction':'S'})
        if path=='/winch':state['winch'].append(parse_qs(r.request.post_data or '').get('direction',[''])[0]);return r.fulfill(status=200 if state['winch_ok'] else 503,body='OK' if state['winch_ok'] else 'unavailable')
        if path=='/tilt/zero':return r.fulfill(status=200 if state['tilt_ok'] else 409,body='OK' if state['tilt_ok'] else 'Tilt sensor unavailable')
        if path=='/playlist':return r.fulfill(json=[])
        if path=='/screen':return r.fulfill(status=204)
        if path in ('/','/dashboard.html','/pairing.css','/pairing.js','/dashboard-polish.css') or path.endswith('.jpg'):return r.continue_()
        return r.fulfill(status=200,body='OK')
    page.route('**/*',route);page.goto(f'http://127.0.0.1:{server.server_address[1]}/dashboard.html')
    page.get_by_text('Vehicle status',exact=True).wait_for()
    assert page.locator('#vehicle-pitch').inner_text()=='12.4°'
    assert page.locator('#vehicle-roll').inner_text()=='−5.1°'
    assert page.locator('#vehicle-battery').inner_text()=='—'
    assert 'Not available' in page.locator('#vehicle-battery-note').inner_text()
    assert page.locator('#remote-connection').inner_text()=='Connected'
    page.locator('.vehicle-card').screenshot(path=str(Path(tempfile.gettempdir())/'magoo-vehicle-preview.png'))
    page.locator('#winch-controls').screenshot(path=str(Path(tempfile.gettempdir())/'magoo-winch-preview.png'))
    b=page.locator('#winch-out');b.scroll_into_view_if_needed();box=b.bounding_box();x=box['x']+box['width']/2;y=box['y']+box['height']/2
    page.mouse.move(x,y);page.mouse.down();page.wait_for_timeout(300)
    assert 'active' in b.get_attribute('class').split()
    assert 'Out' in page.locator('#winch-status').inner_text()
    page.mouse.move(x,y-150);page.mouse.up();page.wait_for_timeout(400)
    assert 'active' not in b.get_attribute('class').split() and state['winch'][-1]=='stop',state['winch']
    assert state['winch'][0]=='out'
    state['winch_ok']=False;page.mouse.move(x,y);page.mouse.down();page.wait_for_timeout(350)
    assert 'Connection lost' in page.locator('#winch-status').inner_text()
    assert 'active' not in b.get_attribute('class').split()
    page.mouse.up();state['winch_ok']=True
    page.locator('#tilt-zero').click();page.wait_for_timeout(400)
    page.get_by_text('Level saved',exact=True).wait_for()
    page.wait_for_timeout(1900);state['tilt_ok']=False
    page.locator('#tilt-zero').click();page.get_by_text('Tilt sensor unavailable',exact=True).wait_for()
    telemetry.update(tiltValid=False,throttleValid=False,batteryValid=True,batteryVoltage=12.37)
    page.evaluate('update()');page.wait_for_timeout(300)
    assert page.locator('#vehicle-pitch').inner_text()=='—'
    assert page.locator('#vehicle-battery').inner_text()=='12.4 V'
    assert page.locator('#remote-connection').inner_text()=='Disconnected'
    state['online']=False;page.evaluate('update()');page.wait_for_timeout(300)
    assert page.locator('#remote-connection').inner_text()=='Unknown'
    assert page.locator('#vehicle-battery').inner_text()=='—'
    page.set_viewport_size({'width':320,'height':700})
    assert page.locator('.vehicle-card').evaluate('el=>el.scrollWidth<=el.clientWidth')
    page.set_viewport_size({'width':1200,'height':900})
    assert page.locator('#winch-controls').evaluate('el=>el.scrollWidth<=el.clientWidth')
    assert not errors,errors
    print('PASS: truthful live status, unavailable sensors, hold-to-move/release-to-stop, and calibration feedback')
    browser.close()
server.shutdown()
