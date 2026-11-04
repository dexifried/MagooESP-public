#include "Winch.h"
#include "WinchSafety.h"
#include <Arduino.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>

namespace {
// Channel 7 (timer 3) drives the backlight; keep the winch on a separate timer.
constexpr int WINCH_PIN = 5, PWM_CHANNEL = 0;
constexpr uint32_t FREQUENCY = 50, RESOLUTION = 16;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
WinchSafety state;
bool ready = false;
uint32_t commandCount = 0;

uint32_t duty(uint16_t pulseUs) { return uint32_t(pulseUs) * 65536UL / 20000UL; }
void runWinch(void*) {
  for (;;) {
    portENTER_CRITICAL(&mux);
    const uint16_t pulse = state.pulse(millis());
    portEXIT_CRITICAL(&mux);
    ledcWrite(PWM_CHANNEL, duty(pulse));
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}

bool winchBegin() {
  if (!ledcSetup(PWM_CHANNEL, FREQUENCY, RESOLUTION)) return false;
  ledcWrite(PWM_CHANNEL, duty(1500));
  ledcAttachPin(WINCH_PIN, PWM_CHANNEL);
  if (xTaskCreatePinnedToCore(runWinch, "winch", 2048, nullptr, 2, nullptr, 1) != pdPASS) {
    ledcDetachPin(WINCH_PIN);
    pinMode(WINCH_PIN, INPUT);
    return false;
  }
  ready = true;
  return true;
}
bool winchSet(char direction, uint8_t speed) {
  if (!ready || (direction != 'S' && direction != 'I' && direction != 'O')) return false;
  portENTER_CRITICAL(&mux); state.set(direction, millis(), speed); ++commandCount; portEXIT_CRITICAL(&mux);
  return true;
}
char winchDirection() {
  portENTER_CRITICAL(&mux); const char direction = state.direction(millis()); portEXIT_CRITICAL(&mux);
  return direction;
}
void winchSetReversed(bool reversed) {
  portENTER_CRITICAL(&mux); state.set('S', millis()); state.setReversed(reversed); portEXIT_CRITICAL(&mux);
}
bool winchIsReady() { return ready; }
uint32_t winchCommandCount() {
  portENTER_CRITICAL(&mux); const uint32_t count=commandCount; portEXIT_CRITICAL(&mux);
  return count;
}
uint32_t winchOutputFrequency() { return ledcReadFreq(PWM_CHANNEL); }
uint32_t winchOutputDuty() { return ledcRead(PWM_CHANNEL); }
uint32_t winchMeasurePulse() {
  // Observe the existing pin output without changing its LEDC routing.
  if (!ready) return 0;
  const bool inputWasEnabled = REG_GET_BIT(GPIO_PIN_MUX_REG[WINCH_PIN], FUN_IE);
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[WINCH_PIN]);
  const uint32_t pulse = pulseIn(WINCH_PIN, HIGH, 60000);
  if (!inputWasEnabled) PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[WINCH_PIN]);
  return pulse;
}
