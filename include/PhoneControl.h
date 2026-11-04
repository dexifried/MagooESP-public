#pragma once
#include <Arduino.h>
#include "Telemetry.h"
bool phoneControlBegin();
void phoneControlSetFramebuffer(const uint16_t* pixels);
void phoneControlPoll();
bool phoneControlConsumeStartupRequest();
Telemetry phoneControlTelemetry();

constexpr int MAP_WIDTH = 72, MAP_HEIGHT = 52;
struct MapFrame { uint8_t pixels[MAP_WIDTH * MAP_HEIGHT]; float bearing; uint32_t receivedAt; bool valid; bool hasBearing; };
const MapFrame& phoneControlMap();

void phoneControlSetStartupActive(bool active);

bool phoneControlSleeping();
void phoneControlWakeFromThrottle();

bool phoneControlConsumeActivity();
void phoneControlSetDimmed(bool dimmed);
void phoneControlSleepFromIdle();
