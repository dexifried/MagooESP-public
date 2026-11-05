"""Exercise the actual app screen's install flow against a simulated ESP."""
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse
from playwright.sync_api import sync_playwright
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from music_package import build_package

server=ThreadingHTTPServer(("127.0.0.1",0),lambda *a,**kw: SimpleHTTPRequestHandler(*a,directory=str(ROOT/"web"),**kw))
threading.Thread(target=server.serve_forever,daemon=True).start()
data={"speed":0,"sleeping":False,"webUnlocked":True,"dimmed":False,"theme":0,"view":"speedometer","wifi":"connected","ip":"","gps":False,"throttleValid":False,"batterySensorOnline":False,"batteryValid":False,"batteryVoltage":None,"tiltValid":False,"pitch":0,"roll":0,"gearDirection":"-","displayGear":"-","estimatedRpm":0,"gearCalibrated":False,"track":"Song","artist":"Artist","art":0,"position":0,"duration":10,"playing":False,"shuffle":False,"homeStyle":0,"homeLayout":0,"trackIndex":0}
installed=bytearray()
seen=[]
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path=r'C:\Program Files\Google\Chrome\Application\chrome.exe')
    page=browser.new_page(viewport={"width":393,"height":852},is_mobile=True,has_touch=True)
    errors=[]
    page.on("pageerror",lambda e:errors.append(str(e)))
    def route(r):
        path=urlparse(r.request.url).path
        if path=="/status":return r.fulfill(json=data)
        if path=="/modules/music":return r.fulfill(json={"installed":False,"installing":False,"version":1})
        if path.startswith("/modules/music/"):
            seen.append(path)
            form=parse_qs(r.request.post_data or "")
            if path.endswith("/start"):
                assert int(form["size"][0])==len(build_package(ROOT))
                return r.fulfill(json={"token":123})
            if path.endswith("/chunk"):
                assert int(form["token"][0])==123 and int(form["offset"][0])==len(installed)
                installed.extend(bytes.fromhex(form["data"][0]))
                return r.fulfill(body="OK")
            assert path.endswith("/finish") and int(form["token"][0])==123
            assert bytes(installed)==build_package(ROOT)
            return r.fulfill(body="OK")
        if path=="/view":
            assert bytes(installed)==build_package(ROOT), "view switched before package was installed"
            data["view"]="music"
            return r.fulfill(body="OK")
        if path=="/playlist":return r.fulfill(json=[])
        if path=="/winch/status":return r.fulfill(json={"ready":True,"reversed":False,"direction":"S"})
        if path=="/screen":return r.fulfill(status=204)
        if path in ("/dashboard.html","/pairing.css","/dashboard-polish.css","/pairing.js","/frame-delta.js","/music-installer.js","/music-pack.js") or path.endswith(".jpg"):return r.continue_()
        return r.fulfill(body="OK")
    page.route("**/*",route)
    page.goto(f"http://127.0.0.1:{server.server_address[1]}/dashboard.html?app=1")
    page.locator("#view-music").click()
    page.locator("#view-music.selected").wait_for(timeout=15000)
    assert page.locator("#music-install-status").inner_text()=="Music is ready"
    assert seen[0].endswith("/start") and seen[-1].endswith("/finish")
    assert not errors,errors
    browser.close()
server.shutdown()
print("Music installation browser flow passed")
