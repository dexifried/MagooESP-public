#pragma once

inline int wifiInitialSlot(bool home, bool hotspot) {
  return home ? 0 : hotspot ? 1 : -1;
}

inline int wifiNextSlot(int previous, bool home, bool hotspot) {
  if (previous == 0 && hotspot) return 1;
  if (previous == 1 && home) return 0;
  return wifiInitialSlot(home, hotspot);
}

inline bool wifiShouldReturnHome(int active, bool homeSaved, bool homeVisible, bool winchStopped) {
  return active == 1 && homeSaved && homeVisible && winchStopped;
}
