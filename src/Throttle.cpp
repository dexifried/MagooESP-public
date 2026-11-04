#include "Throttle.h"
#include "ThrottleDirection.h"
#include <Arduino.h>
namespace {
constexpr uint8_t THROTTLE_PIN = 16; // RX2; receiver signal must be 3.3 V compatible.
constexpr uint32_t TIMEOUT_US = 250000;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t rising = 0, lastValid = 0;
volatile uint16_t width = 0;
volatile bool highSeen = false;
void IRAM_ATTR capturePulse() {
  const uint32_t now = micros(); const bool high = digitalRead(THROTTLE_PIN);
  portENTER_CRITICAL_ISR(&mux);
  if (high) { rising = now; highSeen = true; }
  else if (highSeen) {
    const uint32_t elapsed = now - rising; highSeen = false;
    if (elapsed >= 800 && elapsed <= 2200) { width = elapsed; lastValid = now; }
  }
  portEXIT_CRITICAL_ISR(&mux);
}
}
void throttleBegin() { pinMode(THROTTLE_PIN, INPUT); attachInterrupt(digitalPinToInterrupt(THROTTLE_PIN), capturePulse, CHANGE); }
ThrottleReading throttleRead() {
  uint32_t last; uint16_t pulse;
  portENTER_CRITICAL(&mux); last = lastValid; pulse = width; portEXIT_CRITICAL(&mux);
  if (!pulse || (uint32_t)(micros() - last) > TIMEOUT_US) return {false, 0, 0, 0};
  return {true, pulse, throttleEstimatedSpeed(pulse, 1500), last};
}
