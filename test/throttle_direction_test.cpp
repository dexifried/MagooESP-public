#include "ThrottleDirection.h"

int main() {
  if (throttleDirection(false, 1500, true, 1500, true) != '-') return 1;
  if (throttleDirection(true, 1700, false, 1500, true) != '-') return 2;
  if (throttleDirection(true, 1500, true, 1500, true) != 'N') return 3;
  if (throttleDirection(true, 1540, true, 1500, true) != 'N') return 4;
  if (throttleDirection(true, 1600, true, 1500, true) != 'D') return 5;
  if (throttleDirection(true, 1400, true, 1500, true) != 'R') return 6;
  if (throttleDirection(true, 1400, true, 1500, false) != 'D') return 7;
  if (throttleDirection(true, 1600, true, 1500, false) != 'R') return 8;
  if (throttleDirection(true, 1600, true, 1600, true) != 'N') return 9;
  if (throttleDirection(true, 1641, true, 1600, true) != 'D') return 10;
  if (throttleEstimatedSpeed(1600, 1600) != 0.0f) return 11;
  if (throttleEstimatedSpeed(1640, 1600) != 0.0f) return 12;
  if (throttleEstimatedSpeed(1560, 1600) != 0.0f) return 13;
  if (throttleEstimatedSpeed(2100, 1600) != 10.0f) return 14;
  if (throttleEstimatedSpeed(1100, 1600) != 10.0f) return 15;
  return 0;
}
