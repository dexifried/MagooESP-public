# Magoo in Expo Go

This project runs the same dashboard and app icons inside Expo Go on iPhone or Android. The dashboard assets are bundled with the project; only device API requests and the display mirror go to the ESP. The existing Android APK remains a separate option.

## Start

1. Install Expo Go with SDK 58 support on the phone.
2. In this folder, run `pnpm install` and `pnpm start`.
3. Connect the phone to the same network as this computer and open the Expo link or scan the QR code in Expo Go. Allow local-network access if prompted.
4. Connect Magoo to the same Wi-Fi. If it isn't found, open Settings → Magoo address and enter `magooesp.local` or the ESP's displayed IP address. On Magoo's own access point use `192.168.4.1`.

Expo Go is a development client and needs the development server available to load the project. A phone connected only to Magoo's access point may not be able to reach the computer at the same time. The independently installed Android APK doesn't require the Expo server.

Phone GPS is opt-in through Phone GPS. Sharing runs only while Expo Go is in the foreground; map tiles require internet. The map represents the phone's location. Tile data is obtained from OpenStreetMap with attribution shown on the dashboard.

Starting Expo regenerates `dashboard.generated.js` from the shared web UI and album art. If the server is already running when those assets change, run `pnpm sync-dashboard` to refresh the bundled copy.
