#pragma once
#include <stdint.h>
struct Telemetry {
  uint8_t theme = 0;
  uint8_t colorTheme = 0;
  uint8_t homeMapStyle = 0;
  bool demo = false;
  const char* trackTitle = "";
  const char* trackArtist = "";
  uint16_t trackPosition = 0, trackDuration = 0;
  uint8_t trackArt = 0;
  bool trackPlaying = false;
  bool musicInstalled = false;
  float speedKph = 0;
  float rpm = 0;
  float voltage = 0;
  bool speedAvailable = true;
  bool throttleMode = true;
  bool rpmAvailable = false;
  bool voltageAvailable = false;
  bool tiltAvailable = false;
  float pitch = 0, roll = 0;
  char driveDirection = '-';
};

