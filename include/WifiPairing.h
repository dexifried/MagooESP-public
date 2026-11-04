#pragma once
#include <stdint.h>

// Track the result of an explicit pairing attempt independently of reconnects.
class WifiPairing {
 public:
  enum State { Idle, Connecting, Connected, Failed };
  void start(int slot, uint32_t id = 0) { slot_ = slot; id_ = id; state_ = Connecting; }
  void finish(int slot, bool success) {
    if (state_ == Connecting && slot == slot_) state_ = success ? Connected : Failed;
  }
  int slot() const { return slot_; }
  uint32_t id() const { return id_; }
  State state() const { return state_; }
  const char* name() const {
    switch (state_) {
      case Connecting: return "connecting";
      case Connected: return "connected";
      case Failed: return "failed";
      default: return "idle";
    }
  }
 private:
  int slot_ = -1;
  uint32_t id_ = 0;
  State state_ = Idle;
};
