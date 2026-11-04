#include "SimulatedDrive.h"
#include <stdio.h>

static int fail(const char* message) { puts(message); return 1; }
int main() {
  SimulatedDrive drive;
  auto neutral = drive.update('N', 0, 0.1f);
  if (neutral.gear != 'N' || neutral.speed != 0 || neutral.rpm != 0) return fail("neutral");
  float previousRpm = 0;
  char previousGear = '1';
  bool redlineSeen = false;
  int upshifts = 0;
  for (int i=0; i<400; ++i) {
    const auto state = drive.update('D', 100, 0.05f);
    if (state.gear < previousGear) return fail("gear decreased under acceleration");
    if (state.gear > previousGear) {
      if (state.gear != previousGear + 1) return fail("gear skipped");
      if (!redlineSeen || previousRpm < 7900) return fail("shift occurred before redline");
      if (state.rpm >= previousRpm) return fail("RPM did not fall after shift");
      previousGear = state.gear;
      redlineSeen = false;
      ++upshifts;
    }
    if (state.rpm >= 7900) redlineSeen = true;
    previousRpm = state.rpm;
  }
  const auto full = drive.update('D',100,0.05f);
  if (upshifts < 2 || full.speed < 99.0f || full.gear < '3' || full.rpm < 7900) return fail("full scale and gears");
  for (int i=0; i<20; ++i) {
    const auto slowed = drive.update('D',40,0.05f);
    if (slowed.gear != full.gear) return fail("gear decreased while slowing");
  }
  const auto reverse = drive.update('R',20,0.05f);
  if (reverse.gear != 'R' || reverse.rpm <= 0) return fail("reverse");
  const auto idle = drive.update('N',0,0.05f);
  if (idle.gear != 'N' || idle.rpm != 0 || idle.speed >= reverse.speed) return fail("neutral deceleration");
  puts("PASS: sequential redline shifts and 0-100 simulated scale");
}
