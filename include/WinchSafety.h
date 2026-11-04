#pragma once
#include <stdint.h>

class WinchSafety {
 public:
  void set(char direction, uint32_t now, uint8_t speed = 100) { direction_ = direction; lastCommand_ = now; speed_ = speed > 100 ? 100 : speed; }
  void setReversed(bool reversed) { reversed_ = reversed; }
  char direction(uint32_t now) const {
    return direction_ != 'S' && uint32_t(now - lastCommand_) <= 900 ? direction_ : 'S';
  }
  uint16_t pulse(uint32_t now) const {
    char current = direction(now);
    if (current == 'S') return 1500;
    if (reversed_) current = current == 'O' ? 'I' : 'O';
    const uint16_t offset = 400U * speed_ / 100U;
    return current == 'O' ? 1500 - offset : 1500 + offset;
  }
 private:
  uint32_t lastCommand_ = 0;
  char direction_ = 'S';
  bool reversed_ = false;
  uint8_t speed_ = 100;
};
