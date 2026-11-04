#pragma once
#include <stdint.h>

bool winchBegin();
bool winchSet(char direction, uint8_t speed = 100);
char winchDirection();
void winchSetReversed(bool reversed);
bool winchIsReady();
uint32_t winchCommandCount();
uint32_t winchOutputFrequency();
uint32_t winchOutputDuty();
uint32_t winchMeasurePulse();
