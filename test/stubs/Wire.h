#pragma once
#include <stdint.h>
struct FakeWire {
  bool device = false, initialized = false;
  bool reversedOnly = false;
  int failedBegins = 0, probes = 0, begins = 0, ends = 0;
  int sda = -1, scl = -1;
  uint8_t address = 0, reg = 0, readIndex = 0, count = 0;
  bool begin(int data, int clock, uint32_t) { ++begins; sda=data; scl=clock; if (failedBegins) { --failedBegins; return false; } return initialized = true; }
  bool end() { ++ends; initialized = false; return true; }
  void setTimeOut(int) {}
  void beginTransmission(uint8_t a) { address = a; count = 0; }
  void write(uint8_t v) { if (!count) reg = v; ++count; }
  int endTransmission(bool stop = true) {
    if (!count && stop) ++probes;
    return initialized && device && address == 0x40 && (!reversedOnly || (sda == 13 && scl == 25)) ? 0 : 2;
  }
  uint8_t requestFrom(uint8_t a, uint8_t n) { readIndex = 0; return initialized && device && a == 0x40 ? n : 0; }
  int read() { const uint16_t value = reg == 0 ? 0x399f : 0x5d98; return readIndex++ ? value & 255 : value >> 8; }
};
extern FakeWire Wire1;
