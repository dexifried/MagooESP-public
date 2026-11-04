#pragma once
#include "ThrottleWake.h"

inline void runThrottleWakeSelfTest() {
  int passed = 0, failed = 0;
  auto check = [&](bool ok) { if (ok) ++passed; else ++failed; };
  ThrottleWake detector;
  for (uint32_t i = 0; i < 8; ++i) check(!detector.update(true, 1500, 100000 + i * 20000, 100000 + i * 20000));
  check(!detector.update(false, 1700, 260000, 260000));
  check(!detector.update(true, 1700, 300000, 300000));
  check(!detector.update(true, 1700, 300000, 420000)); // stale single pulse
  for (uint32_t i = 0; i < 5; ++i) check(!detector.update(true, 1700, 500000 + i * 20000, 500000 + i * 20000));
  check(detector.update(true, 1700, 600000, 600000));
  detector.reset();
  for (uint32_t i = 0; i < 5; ++i) check(!detector.update(true, 1300, 700000 + i * 20000, 700000 + i * 20000));
  check(detector.update(true, 1300, 800000, 800000));
  detector.reset();
  check(!detector.update(true, 1700, 1000000, 1000000));
  check(!detector.update(true, 1500, 1020000, 1020000)); // neutral cancels
  check(!detector.update(true, 1700, 1040000, 1040000));
  check(!detector.update(true, 1700, 1060000, 1060000));
  check(!detector.update(true, 1700, 1080000, 1080000));
  Serial.printf("THROTTLE WAKE SELFTEST: %d passed, %d failed\n", passed, failed);
}
