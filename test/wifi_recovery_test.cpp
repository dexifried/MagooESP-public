#include "WifiRecovery.h"
#include <assert.h>
#include <stdio.h>

int main() {
  assert(wifiInitialSlot(true, true) == 0);
  assert(wifiInitialSlot(false, true) == 1);
  assert(wifiInitialSlot(false, false) == -1);
  assert(wifiNextSlot(0, true, true) == 1);
  assert(wifiNextSlot(1, true, true) == 0);
  assert(wifiNextSlot(0, true, false) == 0);
  assert(wifiNextSlot(1, false, true) == 1);
  assert(wifiNextSlot(1, false, false) == -1);
  assert(wifiShouldReturnHome(1, true, true, true));
  assert(!wifiShouldReturnHome(0, true, true, true));
  assert(!wifiShouldReturnHome(1, true, false, true));
  assert(!wifiShouldReturnHome(1, true, true, false));
  assert(!wifiShouldReturnHome(1, false, true, true));
  puts("PASS: Wi-Fi and hotspot failover and preferred-network return");
}
