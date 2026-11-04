#pragma once
#include <stdint.h>

// Wake only after a steady stream of pulses outside the neutral dead zone.
class ThrottleWake {
 public:
  bool update(bool valid, uint16_t pulseUs, uint32_t sampleUs, uint32_t nowUs) {
    const bool active = valid && (pulseUs < 1420 || pulseUs > 1580);
    if (!active || uint32_t(nowUs - sampleUs) > 60000) { reset(); return false; }
    if (!started_) {
      started_ = true; startUs_ = sampleUs; previousSampleUs_ = sampleUs; samples_ = 1;
    } else if (sampleUs != previousSampleUs_) {
      if (uint32_t(sampleUs - previousSampleUs_) > 60000) {
        startUs_ = sampleUs; samples_ = 1;
      } else if (samples_ < 3) {
        ++samples_;
      }
      previousSampleUs_ = sampleUs;
    }
    return samples_ >= 3 && uint32_t(sampleUs - startUs_) >= 100000;
  }
  void reset() { started_ = false; samples_ = 0; }
 private:
  bool started_ = false;
  uint8_t samples_ = 0;
  uint32_t startUs_ = 0, previousSampleUs_ = 0;
};
