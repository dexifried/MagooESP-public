#include "PhoneControl.h"
#include "DashboardLayout.h"
#include "Throttle.h"
#include "ThrottleDirection.h"
#include "SimulatedDrive.h"
#include "Imu.h"
#include "Battery.h"
#include "Winch.h"
#include "WifiRecovery.h"
#include "WifiPairing.h"
#include "DashboardPage.h"
#include "DASHBOARD_SPEED.h"
#include "DASHBOARD_NAVIGATION.h"
#include "DASHBOARD_MUSIC.h"
#include "DASHBOARD_HOME.h"
#include "AlbumArt.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
namespace {
WebServer server(80); Preferences prefs;
MapFrame mapFrame = {};
bool replayStartup = false;
bool startupActive = false;
bool displaySleeping = false;
bool webUnlocked = false;
bool displayDimmed = false;
bool phoneActivity = false;
bool mirrorHeld = false;
bool winchReversed = false;
uint32_t mirrorResumeAt = 0;
const uint16_t* screenBuffer = nullptr;
WiFiClient mirrorClient;
uint8_t* mirrorSnapshot = nullptr;
size_t mirrorSent = 0;
uint32_t mirrorLastProgress = 0;
constexpr size_t MIRROR_BYTES = 240 * 135 * sizeof(uint16_t);
float selectedSpeed = 0; unsigned selectedTheme = 0, selectedView = 0, homeLayout = 0, homeStyle = 0; bool demo = false, shuffle = false, bootAnimation = true;
uint16_t gearNeutralUs = 1500, pendingGearNeutralUs = 0;
bool gearCalibrated = false, gearForwardHigh = true;
SimulatedDrive simulatedDrive;
SimulatedDriveReading driveReading{0, 0, '-'};
uint32_t lastDriveUpdate = 0;
struct Track { const char* title; const char* artist; uint16_t duration; uint8_t art; };
const Track tracks[] = {{"Lose Yourself", "Eminem", 322, 0}, {"8 Mile", "Eminem", 359, 0}, {"Rabbit Run", "Eminem", 190, 0}, {"Till It's Gone", "Yelawolf", 279, 1}, {"American You", "Yelawolf", 234, 1}, {"Best Friend", "Yelawolf", 311, 1}, {"Lucid Dreams", "Juice WRLD", 240, 2}, {"All Girls Are the Same", "Juice WRLD", 166, 2}, {"Lean Wit Me", "Juice WRLD", 176, 2}};
constexpr int TRACK_COUNT = sizeof(tracks) / sizeof(tracks[0]);
int trackIndex = 0;
int nextTrack() { if (!shuffle) return (trackIndex + 1) % TRACK_COUNT; return (trackIndex + 1 + (int)(esp_random() % (TRACK_COUNT - 1))) % TRACK_COUNT; } uint32_t musicPositionMs = 0, musicLastTick = 0; bool musicPlaying = true;
String pendingSsid, pendingPassword, connectionState = "Not configured", scanJson = "[]";
bool connecting = false, newCredentials = false, wasConnected = false, scanning = false, automaticScan = false;
int pendingSlot = 0, activeSlot = -1, retrySlot = -1;
WifiPairing wifiPairing;
uint32_t nextRetry = 0, nextHomeScan = 0;
uint32_t connectStarted = 0;
String jsonString(const String& value) {
  String out = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    const unsigned char c = value[i];
    if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
    else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
    else out += (char)c;
  }
  return out + "\"";
}
bool acceptsGzip(String encodings) {
  encodings.toLowerCase();
  int start = 0;
  while (start < int(encodings.length())) {
    int end = encodings.indexOf(',', start);
    if (end < 0) end = encodings.length();
    String entry = encodings.substring(start, end); entry.trim();
    const int semicolon = entry.indexOf(';');
    String name = semicolon < 0 ? entry : entry.substring(0, semicolon); name.trim();
    if (name == "gzip") {
      if (semicolon < 0) return true;
      String parameters = entry.substring(semicolon + 1); parameters.trim();
      if (parameters.startsWith("q=")) return parameters.substring(2).toFloat() > 0;
      return true;
    }
    start = end + 1;
  }
  return false;
}
void connectHome(const String& ssid, const String& password, bool saveOnSuccess, int slot = 0) {
  pendingSlot = slot;
  pendingSsid = ssid; pendingPassword = password; newCredentials = saveOnSuccess;
  WiFi.mode(WIFI_AP_STA); WiFi.setAutoReconnect(false); WiFi.disconnect(false, false);
  WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
  connecting = true; wasConnected = false; connectStarted = millis(); connectionState = "Connecting";
}
String savedNetwork(int slot) { return prefs.getString(slot == 1 ? "hot_ssid" : "wifi_ssid", ""); }
bool savedNetworkAvailable(int slot) { if (slot < 0 || slot > 1) return false; const String name = savedNetwork(slot); return name.length() && name != "MagooESP"; }
bool connectSaved(int slot) {
  if (slot < 0 || !savedNetworkAvailable(slot)) return false;
  connectHome(savedNetwork(slot), prefs.getString(slot == 1 ? "hot_pass" : "wifi_pass", ""), false, slot);
  return true;
}
bool startNetworkScan(bool automatic) {
  if (connecting || scanning) return false;
  WiFi.mode(WIFI_AP_STA);
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true, false, false, 120) == WIFI_SCAN_FAILED) return false;
  scanning = true; automaticScan = automatic;
  return true;
}
float demoSpeed() { return 4.0f + 4.0f * sinf(millis() * 0.00018f - 1.0f); }

void finishMirror() {
  mirrorClient.stop();
  free(mirrorSnapshot);
  mirrorSnapshot = nullptr;
  mirrorSent = 0;
}
void serviceMirror() {
  if (!mirrorSnapshot) return;
  if (!mirrorClient.connected() || uint32_t(millis() - mirrorLastProgress) > 8000) { finishMirror(); return; }
  for (int chunk = 0; chunk < 4 && mirrorSnapshot; ++chunk) {
    const size_t length = min(size_t(2048), MIRROR_BYTES - mirrorSent);
    const int written = lwip_send(mirrorClient.fd(), mirrorSnapshot + mirrorSent, length, MSG_DONTWAIT);
    if (written > 0) {
      mirrorSent += size_t(written);
      mirrorLastProgress = millis();
      if (mirrorSent == MIRROR_BYTES) finishMirror();
    } else {
      if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) finishMirror();
      break;
    }
  }
}

}
bool phoneControlBegin() {
  prefs.begin("dashboard", false); selectedTheme = prefs.getUChar("theme", 0); selectedView = prefs.getUChar("view_mode", prefs.getBool("nav_view", selectedTheme == 4) ? 1 : 0); homeLayout = prefs.getUChar("home_layout", 0); homeStyle = prefs.getUChar("home_style", 0); if (homeStyle > 1) homeStyle = 0; bootAnimation = prefs.getBool("boot_anim", true); if (homeLayout > 2) homeLayout = 0; shuffle = prefs.getBool("shuffle", false); if (selectedView > 4) selectedView = 0; if (selectedTheme > 3) selectedTheme = 0; musicLastTick = millis();
  gearNeutralUs = prefs.getUShort("gear_neutral", 1500);
  gearForwardHigh = prefs.getBool("gear_high", true);
  gearCalibrated = prefs.getBool("gear_ready", false) && gearNeutralUs >= 1000 && gearNeutralUs <= 2000;
  winchReversed = prefs.getBool("winch_reverse", false); winchSetReversed(winchReversed);
  WiFi.persistent(false); WiFi.setHostname("magooesp"); WiFi.mode(WIFI_AP);
  if (!WiFi.softAP("MagooESP", "REPLACE_WITH_PRIVATE_PASSWORD")) return false;
  const char* requestHeaders[] = {"Accept-Encoding"};
  server.collectHeaders(requestHeaders, 1);
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    if (displaySleeping || !webUnlocked) { server.send_P(200, "text/html; charset=utf-8", STANDBY_PAGE); return; }
    server.sendHeader("Vary", "Accept-Encoding");
    if (acceptsGzip(server.header("Accept-Encoding"))) {
      server.sendHeader("Content-Encoding", "gzip");
      server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<PGM_P>(DASHBOARD_PAGE_GZIP), DASHBOARD_PAGE_GZIP_SIZE);
    } else server.send_P(200, "text/html; charset=utf-8", DASHBOARD_PAGE);
  });
  server.on("/home.jpg", HTTP_GET, [] { server.sendHeader("Cache-Control", "public, max-age=86400"); server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(DASHBOARD_HOME), DASHBOARD_HOME_SIZE); });
  server.on("/music.jpg", HTTP_GET, [] { server.sendHeader("Cache-Control", "public, max-age=86400"); server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(DASHBOARD_MUSIC), DASHBOARD_MUSIC_SIZE); });
  server.on("/speed.jpg", HTTP_GET, [] { server.sendHeader("Cache-Control", "public, max-age=86400"); server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(DASHBOARD_SPEED), DASHBOARD_SPEED_SIZE); });
  server.on("/navigation.jpg", HTTP_GET, [] { server.sendHeader("Cache-Control", "public, max-age=86400"); server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(DASHBOARD_NAVIGATION), DASHBOARD_NAVIGATION_SIZE); });
  server.on("/hero.jpg", HTTP_GET, [] { server.sendHeader("Cache-Control", "public, max-age=86400"); server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(DASHBOARD_HERO), DASHBOARD_HERO_SIZE); });
  server.on("/app.apk", HTTP_GET, [] { server.sendHeader("Location", "https://magoo.dexifyz.com/"); server.send(302, "text/plain", "Open Magoo download"); });
  server.on("/winch", HTTP_POST, [] {
    const String direction = server.arg("direction");
    const char command = direction == "out" ? 'O' : direction == "in" ? 'I' : direction == "stop" ? 'S' : '?';
    if (command == '?') { server.send(400, "text/plain", "Invalid direction"); return; }
    long speed = 100;
    if (server.hasArg("speed")) {
      const String value = server.arg("speed"); char* end = nullptr;
      speed = strtol(value.c_str(), &end, 10);
      if (end == value.c_str() || *end || speed < 0 || speed > 100) { server.send(400, "text/plain", "Invalid speed"); return; }
    }
    if (!winchSet(command, (uint8_t)speed)) { server.send(503, "text/plain", "Winch unavailable"); return; }
    phoneActivity = true; server.send(200, "text/plain", "OK");
  });
  server.on("/winch/reverse", HTTP_POST, [] {
    const String value = server.arg("reversed");
    if (value != "0" && value != "1") { server.send(400, "text/plain", "Invalid setting"); return; }
    winchReversed = value == "1"; winchSetReversed(winchReversed);
    prefs.putBool("winch_reverse", winchReversed); phoneActivity = true;
    server.send(200, "text/plain", "OK");
  });
  server.on("/winch/status", HTTP_GET, [] {
    server.send(200, "application/json", String("{\"ready\":") + (winchIsReady() ? "true" : "false") + ",\"reversed\":" + (winchReversed ? "true" : "false") + ",\"direction\":" + jsonString(String(winchDirection())) + ",\"frequency\":" + String(winchOutputFrequency()) + ",\"duty\":" + String(winchOutputDuty()) + ",\"commands\":" + String(winchCommandCount()) + "}");
  });
  server.on("/winch/measure", HTTP_GET, [] {
    server.send(200, "application/json", String("{\"pulseUs\":") + String(winchMeasurePulse()) + ",\"frequency\":" + String(winchOutputFrequency()) + ",\"direction\":" + jsonString(String(winchDirection())) + "}");
  });
  // Probe only the already configured IMU bus; do not alter sensor registers.
  server.on("/diagnostics/i2c", HTTP_GET, [] {
    String json = String("{\"secondaryControllerImuProbe\":") + String(imuSecondaryControllerProbe()) + ",\"imuBus\":[";
    bool first = true;
    for (uint8_t address = 0x40; address <= 0x69; ++address) {
      if (address > 0x4f && address < 0x68) continue;
      Wire.beginTransmission(address);
      if (Wire.endTransmission() != 0) continue;
      if (!first) json += ',';
      first = false;
      json += String(address);
    }
    json += "],\"batteryBus\":[";
    first = true;
    for (uint8_t address = 0x40; address <= 0x4f; ++address) {
      Wire1.beginTransmission(address);
      const int result = Wire1.endTransmission();
      if (!first) json += ',';
      first = false;
      json += String(result);
    }
    json += "],\"gpio25\":" + String(digitalRead(25)) + ",\"gpio13\":" + String(digitalRead(13)) + "}";
    server.send(200, "application/json", json);
  });
  server.on("/status", HTTP_GET, [] {
    const ThrottleReading throttle = throttleRead();
    const ImuReading tilt = imuReading();
    const BatteryReading battery = batteryReading();
    const char direction = throttleDirection(throttle.valid, throttle.pulseUs, gearCalibrated, gearNeutralUs, gearForwardHigh);
    String json = String("{\"speed\":") + String(driveReading.speed, 1) + ",\"sleeping\":" + (displaySleeping ? "true" : "false") + ",\"webUnlocked\":" + (webUnlocked ? "true" : "false") + ",\"dimmed\":" + (displayDimmed ? "true" : "false") + ",\"theme\":" + String(selectedTheme) + ",\"view\":\"" + (selectedView == 4 ? "tilt" : (selectedView == 3 ? "home" : (selectedView == 2 ? "music" : (selectedView == 1 ? "navigation" : "speedometer")))) + "\",\"demo\":" + (!throttle.valid ? "true" : "false") + ",\"wifi\":" + jsonString(connectionState) + ",\"ip\":" + jsonString(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("")) + ",\"gps\":" + ((mapFrame.valid && (uint32_t)(millis() - mapFrame.receivedAt) < 30000) ? "true" : "false") + ",\"throttleValid\":" + (throttle.valid ? "true" : "false") + ",\"batterySensorOnline\":" + (battery.sensorOnline ? "true" : "false") + ",\"batteryValid\":" + (battery.voltageAvailable ? "true" : "false") + ",\"batteryVoltage\":" + (battery.sensorOnline ? String(battery.voltage, 3) : "null") + ",\"tiltValid\":" + (tilt.valid ? "true" : "false") + ",\"pitch\":" + String(tilt.pitch, 1) + ",\"roll\":" + String(tilt.roll, 1) + ",\"gearDirection\":" + jsonString(String(direction)) + ",\"displayGear\":" + jsonString(String(driveReading.gear)) + ",\"estimatedRpm\":" + String(driveReading.rpm, 0) + ",\"gearCalibrated\":" + (gearCalibrated ? "true" : "false") + ",\"gearNeutralCaptured\":" + (pendingGearNeutralUs ? "true" : "false") + ",\"throttlePulse\":" + String(throttle.pulseUs) + ",\"track\":" + jsonString(tracks[trackIndex].title) + ",\"artist\":" + jsonString(tracks[trackIndex].artist) + ",\"art\":" + String(tracks[trackIndex].art) + ",\"position\":" + String(musicPositionMs / 1000) + ",\"duration\":" + String(tracks[trackIndex].duration) + ",\"playing\":" + (musicPlaying ? "true" : "false") + ",\"shuffle\":" + (shuffle ? "true" : "false") + ",\"homeStyle\":" + String(homeStyle) + ",\"bootAnimation\":" + (bootAnimation ? "true" : "false") + ",\"homeLayout\":" + String(homeLayout) + ",\"trackIndex\":" + String(trackIndex) + "}";
    server.send(200, "application/json", json);
  });
  server.on("/map", HTTP_POST, [] {
    if (!server.hasArg("pixels") || !server.hasArg("bearing")) { server.send(400, "text/plain", "Missing map"); return; }
    const String pixels = server.arg("pixels");
    if (pixels.length() != MAP_WIDTH * MAP_HEIGHT * 2) { server.send(400, "text/plain", "Invalid map size"); return; }
    for (size_t i = 0; i < pixels.length(); ++i) {
      const char c = pixels[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { server.send(400, "text/plain", "Invalid map pixels"); return; }
    }
    const String direction = server.arg("bearing"); char* end = nullptr;
    const float bearing = strtof(direction.c_str(), &end);
    if (end == direction.c_str() || *end || !isfinite(bearing) || bearing < -1 || bearing >= 360) { server.send(400, "text/plain", "Invalid bearing"); return; }
    uint32_t fixAgeMs = 0;
    if (server.hasArg("ageMs")) {
      const String age = server.arg("ageMs"); char* ageEnd = nullptr;
      const unsigned long parsed = strtoul(age.c_str(), &ageEnd, 10);
      if (!age.length() || age[0] < '0' || age[0] > '9' || *ageEnd || parsed > 25000) { server.send(400, "text/plain", "Invalid fix age"); return; }
      fixAgeMs = uint32_t(parsed);
    }
    for (size_t i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
      const char hi = pixels[2*i], lo = pixels[2*i+1];
      mapFrame.pixels[i] = ((hi <= '9' ? hi-'0' : hi-'a'+10) << 4) | (lo <= '9' ? lo-'0' : lo-'a'+10);
    }
    mapFrame.hasBearing = bearing >= 0; mapFrame.bearing = bearing;
    mapFrame.receivedAt = millis() - fixAgeMs; mapFrame.valid = true;
    server.send(200, "text/plain", "OK");
  });
  server.on("/screen", HTTP_GET, [] {
    if (displaySleeping) { server.sendHeader("X-Display-State", "sleep"); server.send(204); return; }
    if (startupActive || (mirrorHeld && (int32_t)(millis() - mirrorResumeAt) < 0)) { server.sendHeader("X-Display-State", "startup"); server.send(204); return; }
    mirrorHeld = false;
    if (!screenBuffer) { server.send(503, "text/plain", "Display unavailable"); return; }
    // A private copy keeps the mirror coherent while rendering and sensor
    // sampling continue. Fall back to the original synchronous path if RAM
    // is tight rather than losing the mirror feature.
    bool newSnapshot = false;
    if (!mirrorSnapshot && ESP.getFreeHeap() > MIRROR_BYTES + 70000) {
      mirrorSnapshot = static_cast<uint8_t*>(malloc(MIRROR_BYTES));
      if (mirrorSnapshot) { memcpy(mirrorSnapshot, screenBuffer, MIRROR_BYTES); newSnapshot = true; }
    }
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(MIRROR_BYTES); server.send(200, "application/octet-stream", "");
    if (newSnapshot) {
      mirrorClient = server.client();
      mirrorSent = 0; mirrorLastProgress = millis();
      return;
    }
    WiFiClient client = server.client();
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(screenBuffer);
    size_t sent = 0;
    while (sent < MIRROR_BYTES && client.connected()) {
      const size_t amount = client.write(bytes + sent, min((size_t)2048, MIRROR_BYTES - sent));
      if (!amount) break; sent += amount;
    }
  });
  server.on("/touch", HTTP_POST, [] {
    if (displaySleeping) { server.send(409, "text/plain", "Display is asleep"); return; }
    if (!server.hasArg("x") || !server.hasArg("y")) { server.send(400, "text/plain", "Missing position"); return; }
    const String xs = server.arg("x"), ys = server.arg("y"); char *xe = nullptr, *ye = nullptr;
    long x = strtol(xs.c_str(), &xe, 10), y = strtol(ys.c_str(), &ye, 10);
    if (xe == xs.c_str() || *xe || ye == ys.c_str() || *ye || x < 0 || x >= 240 || y < 0 || y >= 135) { server.send(400, "text/plain", "Invalid position"); return; }
    phoneActivity = true;
    String result = "none";
    const unsigned touchView = selectedView == 3 && homeLayout != 0 ? (homeLayout == 1 ? 1 : 2) : selectedView;
    if (touchView == 3) {
      using namespace DashboardLayout;
      const auto dockHit = [x, y](int center) { return y >= dockTop && x >= center-touchRadius && x <= center+touchRadius; };
      if ((x < 144 && y < 105) || dockHit(mapX)) { selectedView = 1; result = "navigation"; }
      else if (dockHit(musicX)) { selectedView = 2; result = "music"; }
      else if (dockHit(settingsX)) result = "settings";
      else if (dockHit(phoneX)) result = "unavailable";
      else if (x >= 149 && y >= 46 && y < 105) {
        if (y >= 84 && x < 180) { trackIndex = (trackIndex + TRACK_COUNT - 1) % TRACK_COUNT; musicPositionMs = 0; result = "track"; }
        else if (y >= 84 && x > 205) { trackIndex = nextTrack(); musicPositionMs = 0; result = "track"; }
        else if (y >= 84) { musicPlaying = !musicPlaying; result = "track"; }
        else { selectedView = 2; result = "music"; }
        musicLastTick = millis();
      }
      if (result == "music" || result == "navigation") prefs.putUChar("view_mode", selectedView);
    } else if (touchView == 2 && y >= 105) {
      if (x < 80) { trackIndex = (trackIndex + TRACK_COUNT - 1) % TRACK_COUNT; musicPositionMs = 0; }
      else if (x > 160) { trackIndex = nextTrack(); musicPositionMs = 0; }
      else musicPlaying = !musicPlaying;
      musicLastTick = millis(); result = "track";
    }
    server.send(200, "application/json", String("{\"action\":") + jsonString(result) + "}");
  });
  server.on("/speed", HTTP_POST, [] {
    if (!server.hasArg("value")) { server.send(400, "text/plain", "Missing speed"); return; }
    const String value = server.arg("value"); char* end = nullptr; const float speed = strtof(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || !isfinite(speed) || speed < 0 || speed > 10) { server.send(400, "text/plain", "Invalid speed"); return; }
    phoneActivity = true; demo = false; selectedSpeed = speed; server.send(200, "text/plain", "OK");
  });
  server.on("/theme", HTTP_POST, [] { const String value = server.arg("value"); if (value != "0" && value != "1" && value != "2" && value != "3") { server.send(400, "text/plain", "Invalid theme"); return; } phoneActivity = true; selectedTheme = value.toInt(); prefs.putUChar("theme", selectedTheme); server.send(200, "text/plain", "OK"); });
  server.on("/view", HTTP_POST, [] { const String value = server.arg("value"); if (value != "speedometer" && value != "navigation" && value != "music" && value != "home" && value != "tilt") { server.send(400, "text/plain", "Invalid view"); return; } phoneActivity = true; selectedView = value == "tilt" ? 4 : (value == "home" ? 3 : (value == "music" ? 2 : (value == "navigation" ? 1 : 0))); prefs.putUChar("view_mode", selectedView); server.send(200, "text/plain", "OK"); });
  server.on("/tilt/zero", HTTP_POST, [] { if (!imuZero()) { server.send(409, "text/plain", "Tilt sensor unavailable"); return; } phoneActivity = true; server.send(200, "text/plain", "OK"); });
  server.on("/gear/calibrate", HTTP_POST, [] {
    const ThrottleReading throttle = throttleRead();
    if (!throttle.valid) { server.send(409, "text/plain", "No receiver signal detected."); return; }
    const String step=server.arg("step");
    if (step == "neutral") {
      if (throttle.pulseUs < 1000 || throttle.pulseUs > 2000) { server.send(400, "text/plain", "Neutral signal is outside the expected range."); return; }
      pendingGearNeutralUs=throttle.pulseUs;
      phoneActivity=true;
      server.send(200, "text/plain", "Neutral saved. Hold forward and tap Set forward.");
      return;
    }
    if (step == "forward") {
      if (!pendingGearNeutralUs) { server.send(409, "text/plain", "Set neutral first."); return; }
      const int delta=int(throttle.pulseUs)-int(pendingGearNeutralUs);
      if (abs(delta) < 100) { server.send(409, "text/plain", "Hold forward a little farther, then try again."); return; }
      const bool high=delta>0;
      // Mark the new setting valid only after both values have been saved.
      if (!prefs.putBool("gear_ready", false) || !prefs.putUShort("gear_neutral", pendingGearNeutralUs) ||
          !prefs.putBool("gear_high", high) || !prefs.putBool("gear_ready", true)) {
        gearCalibrated=false;
        server.send(500, "text/plain", "Could not save calibration."); return;
      }
      gearNeutralUs=pendingGearNeutralUs; gearForwardHigh=high;
      pendingGearNeutralUs=0; gearCalibrated=true; phoneActivity=true;
      server.send(200, "text/plain", "Direction calibrated."); return;
    }
    server.send(400, "text/plain", "Invalid calibration step.");
  });
  server.on("/appearance", HTTP_POST, [] { const String value = server.arg("style"); if (value != "0" && value != "1") { server.send(400, "text/plain", "Invalid style"); return; } phoneActivity = true; homeStyle = value.toInt(); prefs.putUChar("home_style", homeStyle); server.send(200, "text/plain", "OK"); });
  server.on("/sleep", HTTP_POST, [] {
    const String value = server.arg("sleep");
    if (value != "0" && value != "1") { server.send(400, "text/plain", "Invalid sleep state"); return; }
    displaySleeping = value == "1";
    if (!displaySleeping) { phoneActivity = true; if (server.arg("web") == "1") webUnlocked = true; }
    if (displaySleeping) { replayStartup = false; webUnlocked = false; }
    server.send(200, "text/plain", "OK");
  });
  server.on("/replay-startup", HTTP_POST, [] { if (displaySleeping) { server.send(409, "text/plain", "Wake display first"); return; } phoneActivity = true; replayStartup = true; server.send(200, "text/plain", "OK"); });
  server.on("/startup", HTTP_POST, [] { const String value = server.arg("animated"); if (value != "0" && value != "1") { server.send(400, "text/plain", "Invalid startup"); return; } phoneActivity = true; bootAnimation = value == "1"; prefs.putBool("boot_anim", bootAnimation); server.send(200, "text/plain", "OK"); });
  server.on("/layout", HTTP_POST, [] { const String value = server.arg("value"); if (value != "0" && value != "1" && value != "2") { server.send(400, "text/plain", "Invalid layout"); return; } phoneActivity = true; homeLayout = value.toInt(); prefs.putUChar("home_layout", homeLayout); server.send(200, "text/plain", "OK"); });
  server.on("/playlist", HTTP_GET, [] { String json = "["; for (int i=0; i<TRACK_COUNT; i++) { if (i) json += ","; json += String("{\"title\":") + jsonString(tracks[i].title) + ",\"artist\":" + jsonString(tracks[i].artist) + "}"; } json += "]"; server.send(200, "application/json", json); });
  server.on("/music", HTTP_POST, [] { const String action = server.arg("action"); if (action == "next") { trackIndex = nextTrack(); musicPositionMs = 0; } else if (action == "previous") { trackIndex = (trackIndex + TRACK_COUNT - 1) % TRACK_COUNT; musicPositionMs = 0; } else if (action == "toggle") { musicPlaying = !musicPlaying; } else if (action == "shuffle") { shuffle = !shuffle; prefs.putBool("shuffle", shuffle); } else if (action == "select") { const String item=server.arg("track"); char* end=nullptr; long n=strtol(item.c_str(), &end, 10); if (end==item.c_str() || *end || n<0 || n>=TRACK_COUNT) { server.send(400, "text/plain", "Invalid track"); return; } trackIndex=(int)n; musicPositionMs=0; musicPlaying=true; } else { server.send(400, "text/plain", "Invalid action"); return; } musicLastTick = millis(); phoneActivity = true; server.send(200, "text/plain", "OK"); });
  server.on("/album/0.jpg", HTTP_GET, [] { server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(JPG_EIGHT), sizeof(JPG_EIGHT)); });
  server.on("/album/1.jpg", HTTP_GET, [] { server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(JPG_LOVE), sizeof(JPG_LOVE)); });
  server.on("/album/2.jpg", HTTP_GET, [] { server.send_P(200, "image/jpeg", reinterpret_cast<PGM_P>(JPG_GOODBYE), sizeof(JPG_GOODBYE)); });
  server.on("/mode", HTTP_POST, [] { const String value = server.arg("demo"); if (value != "0" && value != "1") { server.send(400, "text/plain", "Invalid mode"); return; } phoneActivity = true; demo = value == "1"; if (!demo) selectedSpeed = 0; server.send(200, "text/plain", "OK"); });
  server.on("/scan", HTTP_POST, [] {
    if (server.client().localIP() != WiFi.softAPIP()) { server.send(403, "text/plain", "Connect to MagooESP"); return; }
    if (connecting || scanning) { server.send(409, "text/plain", "Wi-Fi busy"); return; }
    if (!startNetworkScan(false)) { server.send(500, "text/plain", "Scan failed"); return; }
    phoneActivity = true; server.send(200, "text/plain", "Scanning");
  });
  server.on("/networks", HTTP_GET, [] {
    String saved = "[";
    for (int slot = 0; slot < 2; ++slot) if (savedNetworkAvailable(slot)) {
      if (saved.length() > 1) saved += ',';
      saved += String("{\"ssid\":") + jsonString(savedNetwork(slot)) + ",\"slot\":" + slot + "}";
    }
    saved += ']';
    server.send(200, "application/json", String("{\"scanning\":") + (scanning ? "true" : "false") + ",\"networks\":" + scanJson + ",\"saved\":" + saved + ",\"activeSlot\":" + (WiFi.status() == WL_CONNECTED ? activeSlot : -1) + ",\"setupAccess\":" + (server.client().localIP() == WiFi.softAPIP() ? "true" : "false") + ",\"pairing\":" + jsonString(wifiPairing.name()) + ",\"pairingSlot\":" + wifiPairing.slot() + ",\"pairingId\":" + String(wifiPairing.id()) + "}");
  });
  server.on("/wifi", HTTP_POST, [] {
    if (server.client().localIP() != WiFi.softAPIP()) { server.send(403, "text/plain", "Connect to MagooESP"); return; }
    if (scanning || connecting) { server.send(409, "text/plain", "Wi-Fi is busy"); return; }
    if (!server.hasArg("ssid") || !server.hasArg("password")) { server.send(400, "text/plain", "Missing credentials"); return; }
    const String ssid = server.arg("ssid"), password = server.arg("password"), slot = server.arg("slot");
    if (slot != "" && slot != "0" && slot != "1") { server.send(400, "text/plain", "Invalid slot"); return; }
    uint32_t pairId = 0;
    if (server.hasArg("pairId")) {
      const String value = server.arg("pairId");
      if (!value.length() || value.length() > 10) { server.send(400, "text/plain", "Invalid pairing request"); return; }
      for (size_t i = 0; i < value.length(); ++i) if (value[i] < '0' || value[i] > '9') { server.send(400, "text/plain", "Invalid pairing request"); return; }
      char* end = nullptr; errno = 0; unsigned long parsed = strtoul(value.c_str(), &end, 10);
      if (errno == ERANGE || !end || *end) { server.send(400, "text/plain", "Invalid pairing request"); return; }
      pairId = static_cast<uint32_t>(parsed);
    }
    if (server.arg("useSaved") == "1") {
      const int savedSlot = slot == "1" ? 1 : 0;
      if (!savedNetworkAvailable(savedSlot) || savedNetwork(savedSlot) != ssid) { server.send(400, "text/plain", "Saved network unavailable"); return; }
      wifiPairing.start(savedSlot, pairId); phoneActivity = true; server.send(200, "text/plain", "Connecting"); connectSaved(savedSlot); return;
    }
    if (ssid == "MagooESP" || ssid.length() < 1 || ssid.length() > 32 || password.length() > 63 || (password.length() > 0 && password.length() < 8)) { server.send(400, "text/plain", "Invalid Wi-Fi credentials"); return; }
    const int selectedSlot = slot == "1" ? 1 : 0;
    wifiPairing.start(selectedSlot, pairId); phoneActivity = true; server.send(200, "text/plain", "Connecting"); connectHome(ssid, password, true, selectedSlot);
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); }); server.begin();
  connectSaved(wifiInitialSlot(savedNetworkAvailable(0), savedNetworkAvailable(1)));
  Serial.printf("Phone Wi-Fi: MagooESP; address: http://%s\n", WiFi.softAPIP().toString().c_str()); return true;
}
void phoneControlPoll() {
  const uint32_t driveNow=millis();
  const ThrottleReading throttle=throttleRead();
  const char command=throttle.valid
      ? throttleDirection(true,throttle.pulseUs,gearCalibrated,gearNeutralUs,gearForwardHigh)
      : 'D'; // The existing no-receiver demo simulates forward driving.
  const float target=throttle.valid
      ? throttleEstimatedSpeed(throttle.pulseUs,gearCalibrated ? gearNeutralUs : 1500)*10.0f
      : demoSpeed()*10.0f;
  const float dt=lastDriveUpdate ? uint32_t(driveNow-lastDriveUpdate)*0.001f : 0.05f;
  lastDriveUpdate=driveNow;
  driveReading=simulatedDrive.update(command,target,dt);
  server.handleClient();
  serviceMirror();
  const uint32_t musicNow = millis();
  if (musicPlaying) {
    musicPositionMs += musicNow - musicLastTick;
    if (musicPositionMs >= (uint32_t)tracks[trackIndex].duration * 1000) { musicPositionMs = 0; trackIndex = nextTrack(); }
  }
  musicLastTick = musicNow;
  if (scanning) {
    const int count = WiFi.scanComplete();
    if (count != WIFI_SCAN_RUNNING) {
      Serial.printf("Wi-Fi scan result: %d\n", count);
      scanJson = "["; bool first = true, homeVisible = false;
      for (int i = 0; i < count; ++i) {
        String name = WiFi.SSID(i); if (!name.length() || name == "MagooESP") continue;
        if (name == savedNetwork(0)) homeVisible = true;
        bool duplicate = false; for (int j = 0; j < i; ++j) if (WiFi.SSID(j) == name) { duplicate = true; break; }
        if (duplicate) continue; if (!first) scanJson += ","; first = false;
        scanJson += String("{\"ssid\":") + jsonString(name) + ",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"secured\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
      }
      scanJson += "]"; WiFi.scanDelete(); scanning = false;
      if (automaticScan && count >= 0 && WiFi.status() == WL_CONNECTED &&
          wifiShouldReturnHome(activeSlot, savedNetworkAvailable(0), homeVisible, winchDirection() == 'S')) {
        nextHomeScan = millis() + 120000;
        connectSaved(0);
      }
      automaticScan = false;
    }
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    if (connecting) wifiPairing.finish(pendingSlot, true);
    if (connecting && newCredentials) { prefs.putString(pendingSlot ? "hot_ssid" : "wifi_ssid", pendingSsid); prefs.putString(pendingSlot ? "hot_pass" : "wifi_pass", pendingPassword); }
    if (connecting) activeSlot = pendingSlot;
    pendingPassword = ""; connecting = false; newCredentials = false; nextRetry = 0; connectionState = "Connected"; WiFi.setAutoReconnect(false);
    if (activeSlot == 1 && (!nextHomeScan || (int32_t)(millis() - nextHomeScan) >= 0)) nextHomeScan = millis() + 60000;
    MDNS.end(); if (MDNS.begin("magooesp")) MDNS.addService("http", "tcp", 80);
    Serial.printf("Home Wi-Fi connected; IP: %s; host: magooesp.local\n", WiFi.localIP().toString().c_str());
  } else if (!connected && wasConnected) {
    connectionState = "Disconnected";
    retrySlot = wifiNextSlot(activeSlot, savedNetworkAvailable(0), savedNetworkAvailable(1));
    nextRetry = millis() + 3000;
    Serial.println("Wi-Fi disconnected; switching saved network");
  }
  wasConnected = connected;
  if (connecting && !connected && (uint32_t)(millis() - connectStarted) > 25000) {
    wifiPairing.finish(pendingSlot, false);
    connecting = false; pendingPassword = ""; connectionState = "Connection failed";
    WiFi.disconnect(false, false);
    retrySlot = newCredentials && savedNetworkAvailable(activeSlot) ? activeSlot : wifiNextSlot(pendingSlot, savedNetworkAvailable(0), savedNetworkAvailable(1));
    newCredentials = false;
    nextRetry = millis() + (retrySlot >= 0 && retrySlot != pendingSlot ? 1000 : 60000);
    Serial.println("Wi-Fi connection failed; recovery hotspot available");
  }
  if (!connected && !connecting && !scanning && nextRetry && (int32_t)(millis() - nextRetry) >= 0) {
    nextRetry = 0;
    connectSaved(retrySlot);
  }
  if (connected && !connecting && !scanning && activeSlot == 1 && savedNetworkAvailable(0) &&
      winchDirection() == 'S' && nextHomeScan && (int32_t)(millis() - nextHomeScan) >= 0) {
    nextHomeScan = millis() + 60000;
    startNetworkScan(true);
  }
}
Telemetry phoneControlTelemetry() { Telemetry t; const ThrottleReading throttle = throttleRead(); const ImuReading tilt = imuReading(); const BatteryReading battery = batteryReading(); t.voltageAvailable = battery.voltageAvailable; t.voltage = battery.voltage; t.driveDirection = driveReading.gear; t.tiltAvailable = tilt.valid; t.pitch = tilt.pitch; t.roll = tilt.roll; t.colorTheme = selectedTheme; t.homeMapStyle = homeStyle; t.speedKph = driveReading.speed; t.speedAvailable = true; t.rpm = driveReading.rpm; t.rpmAvailable = true; t.theme = selectedView == 4 ? 7 : selectedView == 3 ? (homeLayout == 1 ? 4 : (homeLayout == 2 ? 5 : 6)) : (selectedView == 2 ? 5 : (selectedView == 1 ? 4 : selectedTheme)); t.demo = !throttle.valid; t.trackTitle=tracks[trackIndex].title; t.trackArtist=tracks[trackIndex].artist; t.trackDuration=tracks[trackIndex].duration; t.trackPosition=musicPositionMs/1000; t.trackArt=tracks[trackIndex].art; t.trackPlaying=musicPlaying; return t; }




const MapFrame& phoneControlMap() { return mapFrame; }


void phoneControlSetFramebuffer(const uint16_t* pixels) { screenBuffer = pixels; }

bool phoneControlConsumeStartupRequest() { const bool requested = replayStartup; replayStartup = false; return requested; }

void phoneControlSetStartupActive(bool active) {
  if (active) mirrorHeld = true;
  else if (startupActive) mirrorResumeAt = millis() + 2000;
  startupActive = active;
}

bool phoneControlSleeping() { return displaySleeping; }

void phoneControlWakeFromThrottle() { displaySleeping = false; }

bool phoneControlConsumeActivity() { const bool active=phoneActivity; phoneActivity=false; return active; }
void phoneControlSetDimmed(bool dimmed) { displayDimmed=dimmed; }
void phoneControlSleepFromIdle() { displaySleeping=true; replayStartup=false; webUnlocked=false; }
