from pathlib import Path
import gzip

def render_page(root=Path('.')):
    page = (root / 'web/dashboard.html').read_text(encoding='utf-8-sig')
    css = (root / 'web/pairing.css').read_text(encoding='utf-8')
    polish_css = (root / 'web/dashboard-polish.css').read_text(encoding='utf-8')
    js = (root / 'web/pairing.js').read_text(encoding='utf-8')
    style_tag = '<link rel="stylesheet" href="/pairing.css">'
    polish_tag = '<link rel="stylesheet" href="/dashboard-polish.css">'
    script_tag = '<script src="/pairing.js"></script>'
    if page.count(style_tag) != 1 or page.count(polish_tag) != 1 or page.count(script_tag) != 1:
        raise ValueError('Dashboard pairing asset tags are missing or duplicated')
    return page.replace(style_tag, '<style>' + css + '</style>').replace(polish_tag, '<style>' + polish_css + '</style>').replace(script_tag, '<script>' + js + '</script>')

def main():
    page = render_page()
    image = Path('web/hero.jpg').read_bytes()
    compressed = gzip.compress(page.encode('utf-8'), compresslevel=9, mtime=0)
    Path('include/DashboardPage.h').write_text('#pragma once\n#include <Arduino.h>\nconst char DASHBOARD_PAGE[] PROGMEM = R"MAGOOHTML(' + page + ')MAGOOHTML";\nconst uint8_t DASHBOARD_PAGE_GZIP[] PROGMEM = {\n' + ','.join(str(byte) for byte in compressed) + '\n};\nconst size_t DASHBOARD_PAGE_GZIP_SIZE = sizeof(DASHBOARD_PAGE_GZIP);\nconst uint8_t DASHBOARD_HERO[] PROGMEM = {\n' + ','.join(str(byte) for byte in image) + '\n};\nconst size_t DASHBOARD_HERO_SIZE = sizeof(DASHBOARD_HERO);\n', encoding='utf-8')

    for name in ('speed', 'navigation', 'music', 'home'):
        data = Path('web/' + name + '.jpg').read_bytes()
        symbol = 'DASHBOARD_' + name.upper()
        Path('include/' + symbol + '.h').write_text('#pragma once\n#include <Arduino.h>\nconst uint8_t ' + symbol + '[] PROGMEM = {\n' + ','.join(str(byte) for byte in data) + '\n};\nconst size_t ' + symbol + '_SIZE = sizeof(' + symbol + ');\n', encoding='utf-8')

if __name__ == '__main__':
    main()
