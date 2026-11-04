#include "Arduino.h"
#include "Wire.h"
#include "Battery.h"
#include <stdio.h>
FakeSerial Serial;
FakeWire Wire1;

int main() {
  Wire1.failedBegins = 1;
  Wire1.device = true;
  batteryBegin();
  for (uint32_t now = 10; now <= 7000; now += 10) {
    const int before = Wire1.probes;
    batteryPoll(now);
    if (Wire1.probes - before > 1) { puts("FAIL: one poll scanned more than one address"); return 1; }
  }
  const BatteryReading reading = batteryReading();
  if (!reading.sensorOnline || !reading.voltageAvailable || reading.voltage < 11.97f || reading.voltage > 11.99f) {
    puts("FAIL: sensor did not recover after initial I2C setup failure"); return 2;
  }
  Wire1.device = false;
  for (uint32_t now = 7010; now <= 14000; now += 10) {
    const int before = Wire1.probes;
    batteryPoll(now);
    if (Wire1.probes - before > 1) { puts("FAIL: missing device blocks with a full scan"); return 3; }
  }
  if (batteryReading().sensorOnline) { puts("FAIL: lost sensor remains online"); return 4; }
  Wire1.reversedOnly = true; Wire1.device = true;
  for (uint32_t now = 14010; now <= 21000; now += 10) {
    const int before = Wire1.probes;
    batteryPoll(now);
    if (Wire1.probes - before > 1) { puts("FAIL: reversed scan is not bounded"); return 5; }
  }
  if (!batteryReading().sensorOnline || Wire1.sda != 13 || Wire1.scl != 25) {
    puts("FAIL: reversed sensor did not reconnect"); return 6;
  }
  puts("PASS: I2C setup retries and scanning remains bounded");
  return 0;
}
