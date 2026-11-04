#pragma once
#include <stdint.h>
struct ImuReading {
  bool valid = false;
  float pitch = 0, roll = 0;
};
void imuBegin();
void imuPoll(uint32_t now);
ImuReading imuReading();
bool imuZero();
int imuSecondaryControllerProbe();
