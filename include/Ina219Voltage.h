#pragma once
#include <stdint.h>

// INA219 bus voltage uses bits 15:3 at 4 mV per count (TI SBOS448G).
inline uint32_t ina219BusMillivolts(uint16_t raw) { return uint32_t(raw >> 3) * 4; }
