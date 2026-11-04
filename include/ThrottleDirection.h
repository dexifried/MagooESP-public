#pragma once
#include <stdint.h>

inline float throttleEstimatedSpeed(uint16_t pulseUs, uint16_t neutralUs) {
  int offset = int(pulseUs) - int(neutralUs);
  if (offset < 0) offset = -offset;
  if (offset <= 40) return 0.0f;
  if (offset >= 500) return 10.0f;
  return (offset - 40) * (10.0f / 460.0f);
}

// This is the receiver's requested direction, not a transmission sensor.
inline char throttleDirection(bool signalValid, uint16_t pulseUs, bool calibrated,
                              uint16_t neutralUs, bool forwardHigh) {
  if (!signalValid || !calibrated) return '-';
  const int offset = int(pulseUs) - int(neutralUs);
  if (offset >= -40 && offset <= 40) return 'N';
  const bool isForward = (offset > 0) == forwardHigh;
  return isForward ? 'D' : 'R';
}
