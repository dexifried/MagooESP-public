#include "Dashboard.h"
#include "DashboardUi.h"
#include "PhoneControl.h"
#include "HomeMap.h"
#include "AlbumArt.h"
#include "StartupArtwork.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <math.h>
#include <stdio.h>

namespace {
constexpr uint16_t BLACK = 0x0000, PANEL = 0x1082, EDGE = 0x39E7;
constexpr uint16_t DIM = 0x8410, WHITE = 0xFFFF, RED = 0xF800, DARK_RED = 0x7800;
constexpr float PI_F = 3.14159265f;

float clamped(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}
void radialLine(GFXcanvas16& c, int cx, int cy, float degrees,
                int inner, int outer, uint16_t color) {
  const float angle = degrees * PI_F / 180.0f;
  const float dx = cosf(angle), dy = sinf(angle);
  c.drawLine(cx + (int)(dx * inner), cy + (int)(dy * inner),
             cx + (int)(dx * outer), cy + (int)(dy * outer), color);
}
// Alpha-composite an antialiased 4-bit mask directly onto the 16-bit canvas.
uint16_t blend565(uint16_t bg, uint16_t fg, unsigned alpha) {
  const unsigned inv = 255 - alpha;
  const unsigned r = (((bg >> 11) & 31) * inv + ((fg >> 11) & 31) * alpha + 127) / 255;
  const unsigned g = (((bg >> 5) & 63) * inv + ((fg >> 5) & 63) * alpha + 127) / 255;
  const unsigned b = ((bg & 31) * inv + (fg & 31) * alpha + 127) / 255;
  return (r << 11) | (g << 5) | b;
}
void startupMask(GFXcanvas16& c, const uint8_t* mask, int w, int h,
                 int x, int y, uint16_t color, unsigned opacity) {
  if (!opacity) return;
  for (int row = 0; row < h; ++row) for (int col = 0; col < w; ++col) {
    const int i = row * w + col;
    const uint8_t packed = pgm_read_byte(mask + i / 2);
    const unsigned coverage = i & 1 ? packed & 15 : packed >> 4;
    if (coverage) {
      const unsigned alpha = coverage * opacity / 15;
      c.drawPixel(x + col, y + row, blend565(c.getPixel(x + col, y + row), color, alpha));
    }
  }
}

void grille(GFXcanvas16& c, int x, int y) {
  c.drawCircle(x, y + 4, 3, DIM);
  c.drawCircle(x + 34, y + 4, 3, DIM);
  for (int i = 0; i < 7; ++i) c.fillRoundRect(x + 5 + i * 4, y, 2, 9, 1, DIM);
}
}
Dashboard::Dashboard(Adafruit_ST7789& display) : display_(display), canvas_(240, 135) {}
bool Dashboard::begin() { return canvas_.getBuffer() != nullptr; }
void Dashboard::present() {
  // The framebuffer is contiguous; transmit it as one SPI window rather than
  // asking drawRGBBitmap to dispatch each of its 135 rows separately.
  display_.startWrite();
  display_.setAddrWindow(0, 0, 240, 135);
  display_.writePixels(canvas_.getBuffer(), 240 * 135);
  display_.endWrite();
}
void Dashboard::startup(uint8_t colorTheme) {
  const uint16_t colors[] = {0xF800, 0x05FF, 0xFD20, 0x07E0};
  startupAccent_ = colors[colorTheme < 4 ? colorTheme : 0];
  startupFrame_ = 0; startupLast_ = 0; startupStarted_ = 0;
}
bool Dashboard::animateStartup(uint32_t now, const Telemetry& data) {
  if (startupFrame_ < 0) return false;
  constexpr int lastFrame = 210;
  if (!startupStarted_) startupStarted_ = now;
  const uint32_t elapsed = now - startupStarted_;
  if (elapsed / 24 > lastFrame) { startupFrame_ = -1; return false; }
  if (startupLast_ && (uint32_t)(now - startupLast_) < 24) return true;
  startupLast_ = now;
  const int frame = elapsed / 24;
  startupFrame_ = frame;
  const auto ease = [](int value) -> unsigned {
    const unsigned t = constrain(value, 0, 255);
    return t * t * (765 - 2 * t) / 65025;
  };
  // Draw the selected view to the framebuffer, then expose it from the bottom.
  // The panel retains the boot screen above the moving edge; no extra framebuffer
  // or intermediate full-screen present is needed.
  constexpr int wipeStart = 170;
  if (frame >= wipeStart) {
    render(data, false);
    const float progress = (frame - wipeStart) / float(lastFrame - wipeStart);
    // Quintic easing gives the wipe a gentle start and finish with a faster middle.
    const float eased = progress * progress * progress * (progress * (progress * 6.0f - 15.0f) + 10.0f);
    const int height = int(eased * 135.0f + 0.5f);
    if (height > 0) {
      const int top = 135 - height;
      display_.startWrite();
      display_.setAddrWindow(0, top, 240, height);
      display_.writePixels(canvas_.getBuffer() + top * 240, 240 * height);
      display_.endWrite();
      // The next frame redraws the view over the previous position of the line.
      display_.drawFastHLine(0, top, 240, RED);
      if (height > 1) display_.drawFastHLine(0, top + 1, 240, DARK_RED);
    }
    return true;
  }
  // Stagger the two titles before revealing the current dashboard view.
  const unsigned fadeIn = ease(frame * 255 / 30);
  const unsigned fadeOut = ease((lastFrame - frame) * 255 / 30);
  const unsigned opacity = min(fadeIn, fadeOut);
  const unsigned captionOpacity = min(ease((frame - 50) * 255 / 30), fadeOut);
  const unsigned lineOpacity = min(ease((frame - 26) * 255 / 26), fadeOut);
  const uint16_t accent = startupAccent_;
  const uint16_t backgroundTop = 0x1083, backgroundBottom = 0x0000;
  for (int y = 0; y < 135; ++y) {
    // A restrained graphite gradient, with a soft theme-colored horizon.
    uint16_t bg = blend565(backgroundTop, backgroundBottom, y * 255 / 134);
    const int distance = abs(y - 84);
    if (distance < 30) bg = blend565(bg, accent, (30 - distance) * opacity / 255 / 3);
    canvas_.drawFastHLine(0, y, 240, blend565(BLACK, bg, fadeOut));
  }
  // Soft halos and bright circular LED lenses flank the Jeep wordmark.
  const unsigned lampOpacity = min(ease((frame - 14) * 255 / 28), fadeOut);
  if (lampOpacity) for (int centerX : {28, 211}) {
    for (int dy = -15; dy <= 15; ++dy) for (int dx = -15; dx <= 15; ++dx) {
      const int distance2 = dx * dx + dy * dy;
      if (distance2 > 225) continue;
      uint16_t color = accent;
      unsigned alpha = (225 - distance2) * 30 / 225;
      if (distance2 <= 100) { color = 0x39E7; alpha = 170; }
      if (distance2 <= 64) { color = 0xBDF7; alpha = 210; }
      if (distance2 <= 36) { color = WHITE; alpha = 255; }
      const int x = centerX + dx, y = 56 + dy;
      canvas_.drawPixel(x, y, blend565(canvas_.getPixel(x, y), color, alpha * lampOpacity / 255));
    }
  }
  const int logoY = 37 + (255 - fadeIn) * 5 / 255;
  startupMask(canvas_, STARTUP_LOGO, STARTUP_LOGO_WIDTH, STARTUP_LOGO_HEIGHT,
              (240 - STARTUP_LOGO_WIDTH) / 2, logoY, WHITE, opacity);
  startupMask(canvas_, STARTUP_CAPTION, STARTUP_CAPTION_WIDTH, STARTUP_CAPTION_HEIGHT,
              (240 - STARTUP_CAPTION_WIDTH) / 2, 88, 0xBDF7, captionOpacity);
  // A narrow light glides across a continuous, softly lit accent line.
  const int sweep = -35 + (frame - 26) * 310 / 100;
  for (int x = 30; x < 210; ++x) {
    const int distance = abs(x - sweep);
    const unsigned glow = distance < 35 ? (35 - distance) * 150 / 35 : 0;
    const unsigned strength = (35 + glow) * lineOpacity / 255;
    canvas_.drawPixel(x, 78, blend565(canvas_.getPixel(x, 78), accent, strength));
    if (distance < 12) canvas_.drawPixel(x, 79, blend565(canvas_.getPixel(x, 79), accent, strength / 3));
  }
  present();
  return true;
}
void Dashboard::render(const Telemetry& data, bool presentFrame) {
  DashboardUi::render(canvas_, data);
  if (presentFrame) present();
}
