#pragma once
#include <math.h>

struct TiltAngles { float pitch, roll; };

// Board X points forward and board Y points to the left of the vehicle.
inline bool tiltFromGravity(float x, float y, float z, TiltAngles& out) {
  const float magnitude = sqrtf(x*x + y*y + z*z);
  if (!isfinite(magnitude) || magnitude < 0.15f || magnitude > 4.0f) return false;
  constexpr float degrees = 57.2957795f;
  out.pitch = atan2f(-x, sqrtf(y*y + z*z)) * degrees;
  out.roll = atan2f(y, z) * degrees;
  return true;
}

inline float tiltDifference(float value, float reference) {
  float result = value - reference;
  while (result > 180.0f) result -= 360.0f;
  while (result < -180.0f) result += 360.0f;
  return result;
}
