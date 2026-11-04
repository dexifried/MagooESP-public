#pragma once
#include <stdint.h>

enum class IdleState { Bright, Dim, Sleep };

// Elapsed times use unsigned subtraction so millis() rollover is safe.
class IdleTimer {
 public:
  explicit IdleTimer(uint32_t dimAfter=300000, uint32_t sleepAfter=600000)
      : dimAfter_(dimAfter), sleepAfter_(sleepAfter) {}
  void activity(uint32_t now) { lastActivity_=now; }
  IdleState state(uint32_t now) const {
    const uint32_t elapsed=now-lastActivity_;
    if (elapsed>=sleepAfter_) return IdleState::Sleep;
    if (elapsed>=dimAfter_) return IdleState::Dim;
    return IdleState::Bright;
  }
 private:
  uint32_t lastActivity_=0, dimAfter_, sleepAfter_;
};
