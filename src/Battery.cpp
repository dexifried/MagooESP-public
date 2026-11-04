#include "Battery.h"
#include "Ina219Voltage.h"
#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr int SDA_PIN=25, SCL_PIN=13;
constexpr uint32_t SAMPLE_MS=500, RETRY_MS=3000, RETRY_MAX_MS=12000, STALE_MS=2000;
uint8_t address=0;
uint32_t lastSample=0, lastProbe=0, lastSuccess=0;
uint32_t retryInterval=RETRY_MS;
uint8_t lastGoodAddress=0;
bool lastGoodReversed=false;
bool online=false, busReady=false, busReversed=false;
enum class ProbePhase { Wait, Remembered, Normal, Reversed };
ProbePhase probePhase=ProbePhase::Wait;
uint8_t candidate=0x40;
BatteryReading reading{};

bool readRegister(uint8_t reg,uint16_t& value) {
  Wire1.beginTransmission(address); Wire1.write(reg);
  if (Wire1.endTransmission(false)!=0 || Wire1.requestFrom(address,uint8_t(2))!=2) return false;
  value=(uint16_t(Wire1.read())<<8) | uint8_t(Wire1.read());
  return true;
}
bool writeRegister(uint8_t reg,uint16_t value) {
  Wire1.beginTransmission(address); Wire1.write(reg);
  Wire1.write(uint8_t(value>>8)); Wire1.write(uint8_t(value));
  return Wire1.endTransmission()==0;
}
bool probe(uint8_t probeAddress) {
    address=probeAddress;
    Wire1.beginTransmission(address);
    if (Wire1.endTransmission()!=0) return false;
    uint16_t config=0;
    if (!readRegister(0,config) || config==0xffff) return false;
    // Leave the sensor's range and gain alone; enable continuous voltage
    // sampling if it was powered down or configured for one-shot conversion.
    if ((config&7)!=6 && (config&7)!=7) {
      if (!writeRegister(0,(config&~uint16_t(7))|uint16_t(7))) return false;
    }
    online=true; reading.sensorOnline=true;
    lastGoodAddress=address;lastGoodReversed=busReversed;retryInterval=RETRY_MS;
    Serial.printf("INA219 detected at 0x%02X; SDA=D%d SCL=D%d\n",address,
                  busReversed ? SCL_PIN : SDA_PIN,busReversed ? SDA_PIN : SCL_PIN);
    return true;
}
bool configureBus(bool reversed) {
  if (busReady) Wire1.end();
  busReady=Wire1.begin(reversed ? SCL_PIN : SDA_PIN,reversed ? SDA_PIN : SCL_PIN,100000);
  busReversed=reversed;
  if (busReady) Wire1.setTimeOut(20);
  return busReady;
}
}

void batteryBegin() {
  busReady=configureBus(false);
  if (!busReady) { Serial.println("INA219 I2C initialization failed; retrying"); return; }
  candidate=0x40; probePhase=ProbePhase::Normal;
}

void batteryPoll(uint32_t now) {
  if (!busReady) {
    if (uint32_t(now-lastProbe)<retryInterval) return;
    lastProbe=now;
    if (!configureBus(false)) return;
    candidate=0x40; probePhase=ProbePhase::Normal;
  }
  if (!online) {
    if (probePhase==ProbePhase::Wait) {
      if (uint32_t(now-lastProbe)<retryInterval) return;
      const bool reverse=lastGoodAddress && lastGoodReversed;
      if (busReversed!=reverse && !configureBus(reverse)) { lastProbe=now; return; }
      candidate=lastGoodAddress ? lastGoodAddress : 0x40;
      probePhase=lastGoodAddress ? ProbePhase::Remembered : ProbePhase::Normal;
    }
    // Probe at most one address per loop so a missing device or a timeout
    // doesn't block the dashboard and phone controls for a whole scan.
    if (probe(candidate)) { lastSuccess=now; probePhase=ProbePhase::Wait; return; }
    if (probePhase==ProbePhase::Remembered) {
      if (busReversed && !configureBus(false)) { lastProbe=now; probePhase=ProbePhase::Wait; return; }
      candidate=0x40;probePhase=ProbePhase::Normal;return;
    }
    if (candidate++<0x4f) return;
    if (probePhase==ProbePhase::Normal) {
      if (!configureBus(true)) { lastProbe=now; probePhase=ProbePhase::Wait; return; }
      candidate=0x40; probePhase=ProbePhase::Reversed;
    } else {
      configureBus(false); address=0; lastProbe=now; probePhase=ProbePhase::Wait;
      retryInterval=retryInterval<RETRY_MAX_MS/2 ? retryInterval*2 : RETRY_MAX_MS;
    }
    return;
  }
  if (uint32_t(now-lastSample)<SAMPLE_MS) return;
  lastSample=now;
  uint16_t raw=0;
  if (!readRegister(2,raw)) {
    if (uint32_t(now-lastSuccess)>=STALE_MS) {
      reading={}; online=false; lastProbe=now;
      Serial.println("INA219 connection lost");
    }
    return;
  }
  lastSuccess=now;
  reading.sensorOnline=true;
  reading.voltage=ina219BusMillivolts(raw)*0.001f;
  // Below 1 V is not a plausible connected 3S pack. Keep the sensor online,
  // but don't label an unconnected sense lead as a real battery reading.
  reading.voltageAvailable=reading.voltage>=1.0f && reading.voltage<=26.0f;
}
BatteryReading batteryReading() { return reading; }
