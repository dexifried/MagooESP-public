#include "Imu.h"
#include "TiltMath.h"
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

namespace {
constexpr int SDA_PIN = 21, SCL_PIN = 22;
uint8_t address = 0;
uint32_t lastProbe = 0, lastSample = 0, lastValid = 0;
int previousProbe[2] = {-1, -1};
int secondaryProbeResult = -1;
float filteredX = 0, filteredY = 0, filteredZ = 0;
float zeroPitch = 0, zeroRoll = 0;
bool filterReady = false;
TiltAngles absoluteAngles{};
ImuReading reading{};

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}
bool readRegs(uint8_t reg, uint8_t* data, uint8_t count) {
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, count) != count) return false;
  for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}
void probe() {
  for (uint8_t candidate : {uint8_t(0x68), uint8_t(0x69)}) {
    address = candidate; uint8_t identity = 0;
    Wire.beginTransmission(address);
    const int ack = Wire.endTransmission();
    const int index = candidate - 0x68;
    if (ack != previousProbe[index]) {
      Serial.printf("MPU probe 0x%02X: %s (%d)\n", candidate, ack == 0 ? "responded" : "no response", ack);
      previousProbe[index] = ack;
    }
    if (ack != 0) continue;
    const bool gotIdentity = readRegs(0x75, &identity, 1);
    const bool supported = (identity & 0x7e) == 0x68 || identity == 0x72;
    if (!gotIdentity) Serial.printf("MPU 0x%02X: identity read failed\n", candidate);
    else if (!supported) Serial.printf("MPU 0x%02X: unexpected identity 0x%02X\n", candidate, identity);
    if (gotIdentity && supported && writeReg(0x6b, 0x00) && writeReg(0x1c, 0x00)) {
      uint8_t power = 0xff, config = 0xff;
      if (!readRegs(0x6b, &power, 1) || (power & 0x40) || !readRegs(0x1c, &config, 1) || (config & 0x18)) {
        Serial.printf("MPU 0x%02X: wake/configuration did not read back\n", candidate);
        continue;
      }
      Serial.printf("MPU-compatible sensor at 0x%02X, ID 0x%02X\n", address, identity);
      return;
    }
  }
  address = 0;
}
int16_t word(const uint8_t* data) { return int16_t((uint16_t(data[0]) << 8) | data[1]); }
}

void imuBegin() {
  Preferences calibration;
  if (calibration.begin("imu", true)) {
    zeroPitch = calibration.getFloat("pitch", 0); zeroRoll = calibration.getFloat("roll", 0);
    calibration.end();
  }
  // Check the second I2C controller against the known IMU bus before either
  // controller is in use, then release it for the separate battery bus.
  if (Wire1.begin(SDA_PIN, SCL_PIN, 100000)) {
    Wire1.setTimeOut(20);
    Wire1.beginTransmission(uint8_t(0x68));
    secondaryProbeResult = Wire1.endTransmission();
    if (secondaryProbeResult != 0) {
      Wire1.beginTransmission(uint8_t(0x69));
      secondaryProbeResult = Wire1.endTransmission();
    }
    Wire1.end();
  }
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(20);
  probe();
  if (!address) Serial.println("MPU-6050 not detected on D21/D22");
}

void imuPoll(uint32_t now) {
  if (!address) {
    if (uint32_t(now-lastProbe) >= 3000) { lastProbe = now; probe(); }
    return;
  }
  if (uint32_t(now-lastSample) < 50) return;
  lastSample = now;
  uint8_t data[6];
  if (!readRegs(0x3b, data, sizeof(data))) {
    if (uint32_t(now-lastValid) > 1000) {
      Serial.println("MPU-6050 connection lost");
      reading.valid = false; address = 0; filterReady = false; lastProbe = now;
    }
    return;
  }
  constexpr float scale = 1.0f / 16384.0f;
  const float x = word(data)*scale, y = word(data+2)*scale, z = word(data+4)*scale;
  if (!filterReady) { filteredX=x; filteredY=y; filteredZ=z; filterReady=true; }
  else { filteredX += 0.2f*(x-filteredX); filteredY += 0.2f*(y-filteredY); filteredZ += 0.2f*(z-filteredZ); }
  if (!tiltFromGravity(filteredX, filteredY, filteredZ, absoluteAngles)) { reading.valid=false; return; }
  lastValid = now;
  // The mounted sensor's axes are swapped relative to the crawler.
  // Apply each saved zero to its original sensor axis before swapping.
  reading.pitch = tiltDifference(absoluteAngles.roll, zeroRoll);
  reading.roll = tiltDifference(absoluteAngles.pitch, zeroPitch);
  reading.valid = true;
}

ImuReading imuReading() { return reading; }
int imuSecondaryControllerProbe() { return secondaryProbeResult; }
bool imuZero() {
  if (!reading.valid) return false;
  Preferences calibration;
  if (!calibration.begin("imu", false)) return false;
  const size_t a=calibration.putFloat("pitch", absoluteAngles.pitch);
  const size_t b=calibration.putFloat("roll", absoluteAngles.roll);
  calibration.end();
  if (a != sizeof(float) || b != sizeof(float)) return false;
  zeroPitch = absoluteAngles.pitch; zeroRoll = absoluteAngles.roll;
  reading.pitch = reading.roll = 0;
  return true;
}
