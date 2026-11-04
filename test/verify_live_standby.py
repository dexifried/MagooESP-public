"""Read-only checks after flashing the standby firmware."""
import json
import time
import urllib.request

opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
for attempt in range(15):
    try:
        with opener.open('http://magooesp.local/status', timeout=3) as response:
            status = json.load(response)
        with opener.open('http://magooesp.local/', timeout=3) as response:
            page = response.read().decode()
        with opener.open('http://magooesp.local/winch/status', timeout=3) as response:
            winch = json.load(response)
        assert not status['sleeping'] and not status['webUnlocked'], 'Expected an awake display and a sleeping website'
        assert 'Hey!' in page and 'https://magoo.dexifyz.com/' in page
        print('PASS: awake display and boot standby page; winch:', winch.get('direction'),
              'gear calibrated:', status.get('gearCalibrated'), 'theme:', status.get('theme'))
        break
    except Exception as error:
        last = error
        time.sleep(2)
else:
    raise SystemExit('Could not verify: ' + str(last))
