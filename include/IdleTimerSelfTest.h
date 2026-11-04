#pragma once
#include "IdleTimer.h"
inline void runIdleTimerSelfTest() {
  int pass=0,fail=0;
  auto check=[&](bool value) { if(value) ++pass; else ++fail; };
  IdleTimer timer;timer.activity(1000);
  check(timer.state(1000)==IdleState::Bright);
  check(timer.state(300999)==IdleState::Bright);
  check(timer.state(301000)==IdleState::Dim);
  check(timer.state(600999)==IdleState::Dim);
  check(timer.state(601000)==IdleState::Sleep);
  timer.activity(450000);check(timer.state(450001)==IdleState::Bright);
  check(timer.state(750000)==IdleState::Dim);
  timer.activity(0xfffffff0u);check(timer.state(0x00000020u)==IdleState::Bright);
  check(timer.state(uint32_t(0xfffffff0u+300000u))==IdleState::Dim);
  check(timer.state(uint32_t(0xfffffff0u+600000u))==IdleState::Sleep);
  Serial.printf("IDLE TIMER SELFTEST: %d passed, %d failed\n",pass,fail);
}
