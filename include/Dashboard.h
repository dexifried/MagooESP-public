#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Telemetry.h"
#include "FrameDelta.h"
#include "DisplayChanges.h"
class Dashboard {
 public:
  explicit Dashboard(Adafruit_ST7789& display);
  bool begin();
  void startup(uint8_t colorTheme);
  void cancelStartup() { startupFrame_ = -1; }
  bool animateStartup(uint32_t now, const Telemetry& data);
  void render(const Telemetry& data, bool presentFrame = true);
  void invalidate() { presentedValid_ = false; changes_.invalidate(); }
  const uint16_t* framebuffer() { return canvas_.getBuffer(); }
 private:
  Adafruit_ST7789& display_;
  GFXcanvas16 canvas_;
  int startupFrame_ = -1;
  uint32_t startupLast_ = 0;
  uint32_t startupStarted_ = 0;
  uint16_t startupAccent_ = 0;
  uint32_t presentedHashes_[FrameDelta::TILE_COUNT]{};
  uint32_t lastFullPresent_ = 0;
  bool presentedValid_ = false;
  DisplayChanges changes_;
  void present();
};
