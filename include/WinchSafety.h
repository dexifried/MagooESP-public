#pragma once
#include <stdint.h>

class WinchSafety {
 public:
  void set(char direction, uint32_t now) { direction_ = direction; lastCommand_ = now; }
  void setReversed(bool reversed) { reversed_ = reversed; }
  char direction(uint32_t now) const {
    return direction_ != 'S' && uint32_t(now - lastCommand_) <= 900 ? direction_ : 'S';
  }
  uint16_t pulse(uint32_t now) const {
    char current = direction(now);
    if (current == 'S') return 1500;
    if (reversed_) current = current == 'O' ? 'I' : 'O';
    return current == 'O' ? 1100 : 1900;
  }
 private:
  uint32_t lastCommand_ = 0;
  char direction_ = 'S';
  bool reversed_ = false;
};
