from pathlib import Path

apk = Path('android-app/app/build/outputs/apk/debug/app-debug.apk').read_bytes()
Path('include/MagooApk.h').write_text(
    '#pragma once\n#include <Arduino.h>\nconst uint8_t MAGOO_APK[] PROGMEM = {\n'
    + ','.join(str(byte) for byte in apk)
    + '\n};\nconst size_t MAGOO_APK_SIZE = sizeof(MAGOO_APK);\n',
    encoding='utf-8',
)
