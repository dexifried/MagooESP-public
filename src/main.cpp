#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <Adafruit_ST7789.h>
#include "BoardConfig.h"
#include "Dashboard.h"
#include "PhoneControl.h"
#include "Throttle.h"
#include "ThrottleWake.h"
#include "IdleTimer.h"
#include "Imu.h"
#include "Battery.h"
#include "Winch.h"
#ifdef TILT_SELFTEST
#define main runTiltMathTest
#include "../test/tilt_math_test.cpp"
#undef main
#endif
#ifdef POWER_IDLE_SELFTEST
#include "IdleTimerSelfTest.h"
#endif
#ifdef THROTTLE_WAKE_SELFTEST
#include "ThrottleWakeSelfTest.h"
#endif
Adafruit_ST7789 display(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Dashboard dashboard(display);
namespace {
constexpr uint8_t BACKLIGHT_CHANNEL=7, BRIGHT=255, DIM=24;
bool backlightPwmReady=false;
void backlight(uint8_t level) {
  if (backlightPwmReady) ledcWrite(BACKLIGHT_CHANNEL,level);
  else digitalWrite(TFT_BL_PIN,level ? HIGH : LOW);
}
}
void setup() {
  Serial.begin(115200);
  if (!winchBegin()) Serial.println("ERROR: winch signal unavailable");
  delay(200);
#ifdef TILT_SELFTEST
  Serial.printf("TILT MATH SELFTEST: %s\n", runTiltMathTest() == 0 ? "PASS" : "FAIL");
#endif
#ifdef THROTTLE_WAKE_SELFTEST
  runThrottleWakeSelfTest();
#endif
#ifdef POWER_IDLE_SELFTEST
  runIdleTimerSelfTest();
#endif
  Serial.println("RC Dashboard boot: phone-controlled display");
  pinMode(TFT_BL_PIN, OUTPUT);
  backlightPwmReady=ledcSetup(BACKLIGHT_CHANNEL,5000,8)>0;
  if (backlightPwmReady) ledcAttachPin(TFT_BL_PIN,BACKLIGHT_CHANNEL);
  else Serial.println("ERROR: Backlight PWM unavailable");
  backlight(BRIGHT);
  SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
  display.init(135, 240, SPI_MODE0);
  display.setRotation(1);
  display.setSPISpeed(40000000);
  if (!dashboard.begin()) {
    Serial.println("ERROR: framebuffer allocation failed");
    while (true) delay(1000);
  }
  Serial.printf("ST7789 initialized: %d x %d; framebuffer allocated\n", display.width(), display.height());
  Preferences startupPrefs;
  startupPrefs.begin("dashboard", true);
  const bool animate = startupPrefs.getBool("boot_anim", true);
  const uint8_t startupColor = startupPrefs.getUChar("theme", 0);
  startupPrefs.end();
  if (animate) dashboard.startup(startupColor);
  phoneControlSetStartupActive(animate);
  throttleBegin();
  imuBegin();
  batteryBegin();
  phoneControlSetFramebuffer(dashboard.framebuffer());
  if (!phoneControlBegin()) Serial.println("ERROR: Wi-Fi access point failed");
  Serial.println("Dashboard running");
}
void loop() {
  static uint32_t lastFrame=0, lastLog=0;
  static bool displayAsleep=false, timerStarted=false, dimmed=false;
  static ThrottleWake throttleWake;
#ifdef IDLE_TEST_TIMING
  static IdleTimer idle(5000,10000);
#else
  static IdleTimer idle;
#endif
  phoneControlPoll();
  const uint32_t now=millis();
  imuPoll(now);
  batteryPoll(now);
  if (!timerStarted) { idle.activity(now); timerStarted=true; }
  if (phoneControlConsumeActivity() && !phoneControlSleeping()) idle.activity(now);
  const ThrottleReading throttle=throttleRead();
  if (throttleWake.update(throttle.valid,throttle.pulseUs,throttle.sampleUs,micros())) {
    idle.activity(now);
    if (phoneControlSleeping()) {
      phoneControlWakeFromThrottle();
      Serial.println("THROTTLE WAKE");
    }
  }
  if (!phoneControlSleeping() && idle.state(now)==IdleState::Sleep) {
    phoneControlSleepFromIdle();
    Serial.println("IDLE SLEEP");
  }
  const bool shouldSleep=phoneControlSleeping();
  if (shouldSleep!=displayAsleep) {
    displayAsleep=shouldSleep;
    if (displayAsleep) {
      dashboard.cancelStartup();
      phoneControlSetStartupActive(false);
      backlight(0);
      display.enableSleep(true);
      dimmed=false;phoneControlSetDimmed(false);
      Serial.println("DISPLAY SLEEP: Wi-Fi active");
    } else {
      idle.activity(now);
      display.enableSleep(false);
      delay(120);  // ST7789 sleep-out recovery before drawing.
      dashboard.render(phoneControlTelemetry());
      backlight(BRIGHT);
      lastFrame=millis();
      Serial.println("DISPLAY AWAKE");
    }
  }
  if (displayAsleep) { delay(10); return; }
  const bool shouldDim=idle.state(now)==IdleState::Dim;
  if (shouldDim!=dimmed) {
    dimmed=shouldDim;
    backlight(dimmed ? DIM : BRIGHT);
    phoneControlSetDimmed(dimmed);
    Serial.println(dimmed ? "IDLE DIM" : "DISPLAY BRIGHT");
  }
  if (phoneControlConsumeStartupRequest()) {
    dashboard.startup(phoneControlTelemetry().colorTheme);
    phoneControlSetStartupActive(true);
  }
  const bool animating=dashboard.animateStartup(now,phoneControlTelemetry());
  phoneControlSetStartupActive(animating);
  if (!animating && now-lastFrame>=50) { lastFrame=now; dashboard.render(phoneControlTelemetry()); }
  if (now-lastLog>=5000) {
    lastLog=now;
    Serial.printf("THROTTLE %s pulse=%u us estimated=%.1f km/h heap=%u\n", throttle.valid ? "SIGNAL" : "NO_SIGNAL",throttle.pulseUs,throttle.speedKph,ESP.getFreeHeap());
  }
  delay(1);
}
