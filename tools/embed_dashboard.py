from pathlib import Path
import gzip
page = Path('web/dashboard.html').read_text(encoding='utf-8-sig')
image = Path('web/hero.jpg').read_bytes()
compressed = gzip.compress(page.encode('utf-8'), compresslevel=9, mtime=0)
Path('include/DashboardPage.h').write_text('#pragma once\n#include <Arduino.h>\nconst char DASHBOARD_PAGE[] PROGMEM = R"MAGOOHTML(' + page + ')MAGOOHTML";\nconst uint8_t DASHBOARD_PAGE_GZIP[] PROGMEM = {\n' + ','.join(str(byte) for byte in compressed) + '\n};\nconst size_t DASHBOARD_PAGE_GZIP_SIZE = sizeof(DASHBOARD_PAGE_GZIP);\nconst uint8_t DASHBOARD_HERO[] PROGMEM = {\n' + ','.join(str(byte) for byte in image) + '\n};\nconst size_t DASHBOARD_HERO_SIZE = sizeof(DASHBOARD_HERO);\n', encoding='utf-8')

for name in ('speed', 'navigation', 'music', 'home'):
    data = Path('web/' + name + '.jpg').read_bytes()
    symbol = 'DASHBOARD_' + name.upper()
    Path('include/' + symbol + '.h').write_text('#pragma once\n#include <Arduino.h>\nconst uint8_t ' + symbol + '[] PROGMEM = {\n' + ','.join(str(byte) for byte in data) + '\n};\nconst size_t ' + symbol + '_SIZE = sizeof(' + symbol + ');\n', encoding='utf-8')
