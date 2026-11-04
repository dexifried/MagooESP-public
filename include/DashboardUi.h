#pragma once
#include <Adafruit_GFX.h>
#include "Telemetry.h"
namespace DashboardUi {
void render(GFXcanvas16& canvas, const Telemetry& data);
}
