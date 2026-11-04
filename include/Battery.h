#pragma once
#include <stdint.h>
struct BatteryReading {
  bool sensorOnline = false;
  bool voltageAvailable = false;
  float voltage = 0;
};
void batteryBegin();
void batteryPoll(uint32_t now);
BatteryReading batteryReading();
