"""Run with Playwright installed; exercises pairing with a simulated ESP."""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import TCPServer
import json
import os
import sys
import tempfile
import threading
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, str(Path(tempfile.gettempdir()) / 'magoo-playwright'))
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
state = {'access': True, 'pairing': 'idle', 'slot': -1, 'id': 0, 'hold_id': True, 'result': 'connected', 'requests': [], 'scanning': False}

class Server(TCPServer):
    allow_reuse_address = True

server = Server(('127.0.0.1', 0), lambda *a, **kw: SimpleHTTPRequestHandler(*a, directory=str(root / 'web'), **kw))
threading.Thread(target=server.serve_forever, daemon=True).start()

with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True, executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe')
    page = browser.new_page(viewport={'width':393, 'height':852}, is_mobile=True, has_touch=True)
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    def intercept(route):
        request = route.request
        path = urlparse(request.url).path
        if path == '/networks':
            return route.fulfill(json={'saved':[{'ssid':'Saved Home','slot':0},{'ssid':'Saved Hotspot','slot':1}],
                'networks':[{'ssid':'Saved Home','secured':True,'rssi':-40},{'ssid':'Trail Wi-Fi','secured':True,'rssi':-50},{'ssid':'Open Wi-Fi','secured':False,'rssi':-65}],
                'activeSlot':1, 'setupAccess':state['access'], 'scanning':state['scanning'], 'pairing':state['pairing'], 'pairingSlot':state['slot'], 'pairingId':state['id']})
        if path == '/scan':
            return route.fulfill(status=200, body='Scanning')
        if path == '/wifi':
            args = parse_qs(request.post_data or '', keep_blank_values=True)
            state['requests'].append(args)
            state['slot'] = int(args['slot'][0]); state['id'] = 0 if state['hold_id'] else int(args['pairId'][0]); state['pairing'] = state['result']
            return route.fulfill(status=200, body='Connecting')
        if path == '/winch/status': return route.fulfill(json={'reversed':False,'ready':True})
        if path == '/playlist': return route.fulfill(json=[])
        if path == '/status': return route.fulfill(status=503, body='unavailable')
        if path == '/screen': return route.fulfill(status=204)
        if path in ('/', '/dashboard.html', '/pairing.css', '/pairing.js') or path.endswith('.jpg'):
            return route.continue_()
        return route.fulfill(status=200, body='OK')
    page.route('**/*', intercept)
    page.goto(f'http://127.0.0.1:{server.server_address[1]}/dashboard.html')
    page.locator('#wifi-open').click()
    page.get_by_role('heading',name='Get Magoo connected').wait_for()
    page.screenshot(path=str(Path(tempfile.gettempdir())/'magoo-pair-welcome.png'))
    page.locator('#pair-continue').click()
    page.get_by_role('button',name='Trail Wi-Fi Password required').wait_for(timeout=10000)
    page.screenshot(path=str(Path(tempfile.gettempdir())/'magoo-pair-networks.png'))
    page.get_by_role('button',name='Trail Wi-Fi Password required').click()
    page.get_by_role('heading',name='Trail Wi-Fi').wait_for()
    page.screenshot(path=str(Path(tempfile.gettempdir())/'magoo-pair-password.png'))
    page.locator('#pair-password').fill('short')
    page.get_by_role('button',name='Connect Magoo',exact=True).click()
    assert page.locator('#pair-form-error').is_visible() and not state['requests']
    page.locator('#pair-password').fill('example-password')
    page.locator('#pair-show-password').click()
    assert page.locator('#pair-password').get_attribute('type') == 'text'
    page.get_by_role('button',name='Connect Magoo',exact=True).click()
    page.get_by_role('heading',name='Connecting Magoo').wait_for()
    page.wait_for_timeout(1300)
    assert page.get_by_role('heading',name='Connecting Magoo').is_visible(), 'Stale success must not be accepted'
    state['id'] = int(state['requests'][-1]['pairId'][0]);state['hold_id'] = False
    page.get_by_role('heading',name='You’re connected').wait_for(timeout=10000)
    assert state['requests'][-1]['ssid']==['Trail Wi-Fi'] and state['requests'][-1]['slot']==['0']
    assert page.locator('#pair-password').input_value()==''
    page.locator('#pair-done').click()
    state.update(result='failed',pairing='idle')
    page.locator('#wifi-open').click();page.locator('#pair-mode-1').click();page.locator('#pair-continue').click()
    page.get_by_role('button',name='Saved Hotspot Connected · saved').wait_for(timeout=10000)
    page.get_by_role('button',name='Saved Hotspot Connected · saved').click()
    assert page.locator('#pair-saved-note').is_visible()
    page.get_by_role('button',name='Connect Magoo',exact=True).click()
    page.get_by_role('heading',name='Let’s try that again').wait_for(timeout=10000)
    assert state['requests'][-1]['useSaved']==['1'] and state['requests'][-1]['password']==['']
    page.locator('#pair-close').click()
    state.update(access=False,pairing='idle')
    page.evaluate('currentTheme = 3')
    page.set_viewport_size({'width':320,'height':700})
    page.locator('#wifi-open').click()
    assert page.locator('#pairing').evaluate("el => getComputedStyle(el).getPropertyValue('--pair-accent').trim()") == '#ef3038'
    assert page.locator('#pairing').evaluate('el => el.scrollWidth <= el.clientWidth')
    page.evaluate('currentTheme = 1; makeThemes()')
    assert page.locator('#pairing').evaluate("el => getComputedStyle(el).getPropertyValue('--pair-accent').trim()") == '#ef3038'
    page.locator('#pair-continue').click()
    page.get_by_role('heading',name='Connect your phone to Magoo').wait_for()
    assert page.locator('#pair-open-setup').is_visible()
    assert not errors, errors
    print('PASS: setup access, scan, password validation, saved credentials, confirmed success and failure')
    browser.close()
server.shutdown()
