#pragma once
#include "Telemetry.h"

// Tracks only inputs used by the selected view; map freshness is time-sensitive.
class DisplayChanges {
 public:
  void invalidate() { valid_=false; }
  bool changed(const Telemetry& t, uint32_t mapRevision, bool mapLive) const {
    if (!valid_ || t.theme!=last_.theme || t.colorTheme!=last_.colorTheme) return true;
    if (t.theme==7)
      return t.tiltAvailable!=last_.tiltAvailable || t.pitch!=last_.pitch || t.roll!=last_.roll;
    if (t.theme==5 || t.theme==6) {
      if (t.musicInstalled!=last_.musicInstalled) return true;
      if (textHash(t.trackTitle)!=titleHash_ || textHash(t.trackArtist)!=artistHash_ ||
          t.trackPosition!=last_.trackPosition || t.trackDuration!=last_.trackDuration ||
          t.trackArt!=last_.trackArt || t.trackPlaying!=last_.trackPlaying) return true;
      if (t.theme==5) return false;
    }
    if (t.theme==4 || t.theme==6) {
      if (mapRevision!=mapRevision_ || mapLive!=mapLive_ || t.homeMapStyle!=last_.homeMapStyle) return true;
      if (t.theme==6) return false;
      return t.speedKph!=last_.speedKph || t.demo!=last_.demo ||
          t.voltageAvailable!=last_.voltageAvailable || t.voltage!=last_.voltage;
    }
    return t.speedKph!=last_.speedKph || t.rpm!=last_.rpm || t.demo!=last_.demo ||
        t.speedAvailable!=last_.speedAvailable || t.rpmAvailable!=last_.rpmAvailable ||
        t.driveDirection!=last_.driveDirection || t.tiltAvailable!=last_.tiltAvailable || t.pitch!=last_.pitch;
  }
  void record(const Telemetry& t, uint32_t mapRevision, bool mapLive) {
    last_=t;mapRevision_=mapRevision;mapLive_=mapLive;
    titleHash_=textHash(t.trackTitle);artistHash_=textHash(t.trackArtist);valid_=true;
  }
 private:
  static uint32_t textHash(const char* text) {
    uint32_t h=2166136261u;if (!text) return h;
    while (*text) h=(h^uint8_t(*text++))*16777619u;
    return h;
  }
  Telemetry last_{};
  uint32_t mapRevision_=0,titleHash_=0,artistHash_=0;
  bool valid_=false,mapLive_=false;
};
