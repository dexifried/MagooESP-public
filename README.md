# Magoo ESP32 dashboard — public source snapshot

This is an automatically updated, sanitized snapshot of the private Magoo project for code review. It contains the ESP32 firmware, dashboard, Android source and tests. It has its own Git history and does not expose the private repository history.

For the parts list and illustrated step-by-step connections, see the [hardware and wiring guide](docs/hardware-guide.md).

The original hotspot password has been replaced in `src/PhoneControl.cpp`; anyone building the firmware must set their own private password. The original home map is replaced with generic geometry. Local captures, backups, binary flash dumps, and the prebuilt Android APK are omitted. The Android app bundles the dashboard and can be built from `android-app`.

Build the ESP32 firmware with `python -m platformio run`. This project drives real RC hardware; review the pin assignments and behavior before flashing any device.

Future idea: smart winch assist is documented in `docs/smart-winch-idea.md`.
