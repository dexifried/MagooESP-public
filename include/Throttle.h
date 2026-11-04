#pragma once
#include <stdint.h>
struct ThrottleReading { bool valid; uint16_t pulseUs; float speedKph; uint32_t sampleUs; };
void throttleBegin();
ThrottleReading throttleRead();
