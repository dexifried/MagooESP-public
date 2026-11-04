#include "Ina219Voltage.h"

int main() {
  if (ina219BusMillivolts(0x5d98) != 11980) return 1;
  if (ina219BusMillivolts(0x5d9b) != 11980) return 2;
  if (ina219BusMillivolts(0) != 0) return 3;
  if (ina219BusMillivolts(0xffff) != 32764) return 4;
  return 0;
}
