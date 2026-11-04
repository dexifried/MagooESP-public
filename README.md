# Magoo ESP32 dashboard — public source snapshot

This is an automatically updated, sanitized snapshot of the private Magoo project for code review. It contains the ESP32 firmware, dashboard, Android source and tests. It has its own Git history and does not expose the private repository history.

The original hotspot password has been replaced in `src/PhoneControl.cpp`; anyone building the firmware must set their own private password. The original home map is replaced with generic geometry. Local captures, backups, binary flash dumps, and the prebuilt Android APK are omitted. To provide the APK, build the Android application and run `python tools/embed_apk.py`.

Build the ESP32 firmware with `python -m platformio run`. This project drives real RC hardware; review the pin assignments and behavior before flashing any device.

Future idea: smart winch assist is documented in `docs/smart-winch-idea.md`.
