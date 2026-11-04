#include "TiltMath.h"
#include <math.h>

static bool near(float a, float b) { return fabsf(a - b) < 0.1f; }
int main() {
  TiltAngles angles{};
  if (!tiltFromGravity(0, 0, 1, angles) || !near(angles.pitch, 0) || !near(angles.roll, 0)) return 1;
  if (!tiltFromGravity(-0.5f, 0, 0.8660254f, angles) || !near(angles.pitch, 30)) return 2;
  if (!tiltFromGravity(0, 0.5f, 0.8660254f, angles) || !near(angles.roll, 30)) return 3;
  if (tiltFromGravity(0, 0, 0, angles)) return 4;
  if (!near(tiltDifference(-179, 179), 2) || !near(tiltDifference(179, -179), -2)) return 5;
  return 0;
}
