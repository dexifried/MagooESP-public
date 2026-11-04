#include "SimulatedDrive.h"
#include <stdio.h>

static int fail(const char* message) { puts(message); return 1; }
int main() {
  if (simulatedThrottleTarget(10.0f,-1.0f)<10.099f || simulatedThrottleTarget(10.0f,-1.0f)>10.101f ||
      simulatedThrottleTarget(10.0f,1.0f)<10.399f || simulatedThrottleTarget(10.0f,1.0f)>10.401f)
    return fail("full throttle should vary from 10.1 to 10.4 km/h");
  SimulatedDrive drive;
  auto neutral = drive.update('N', 0, 0.1f);
  if (neutral.gear != 'N' || neutral.speed != 0 || neutral.rpm != 0) return fail("neutral");
  SimulatedDrive curve;
  float earlyRise=0, lateRise=0, lastCurveRpm=650;
  for (int i=0; i<20; ++i) {
    const auto sample=curve.update('D',10.4f,0.05f);
    if (sample.gear!='1') break;
    const float rise=sample.rpm-lastCurveRpm;
    if (sample.speed>0.3f && sample.speed<0.6f) earlyRise=rise;
    if (sample.speed>1.5f && sample.speed<1.9f) lateRise=rise;
    lastCurveRpm=sample.rpm;
  }
  if (!(earlyRise>lateRise*1.5f && lateRise>0)) return fail("RPM should climb more slowly near redline");
  float previousRpm = 0;
  float previousSpeed = 0;
  char previousGear = '1';
  bool redlineSeen = false;
  int upshifts = 0;
  for (int i=0; i<400; ++i) {
    const auto state = drive.update('D', 10.4f, 0.05f);
    if (previousRpm>=7999 && previousGear<'5' && state.speed>previousSpeed+0.001f)
      return fail("speed should hold during an upshift");
    if (state.gear < previousGear) return fail("gear decreased under acceleration");
    if (state.gear > previousGear) {
      if (state.gear != previousGear + 1) return fail("gear skipped");
      if (!redlineSeen || previousRpm < 7900) return fail("shift occurred before redline");
      if (state.rpm >= previousRpm) return fail("RPM did not fall after shift");
      if (state.rpm > 2500) return fail("RPM should restart low after an upshift");
      previousGear = state.gear;
      redlineSeen = false;
      ++upshifts;
    }
    if (state.rpm >= 7900) redlineSeen = true;
    previousRpm = state.rpm;
    previousSpeed = state.speed;
  }
  const auto full = drive.update('D',10.4f,0.05f);
  if (upshifts != 4 || full.speed < 10.35f || full.speed > 10.4f || full.gear != '5' || full.rpm < 7900) return fail("crawler speed and five gears");
  char downGear = full.gear;
  float downRpm = full.rpm;
  int downshifts = 0;
  for (int i=0; i<100; ++i) {
    const auto slowed = drive.update('N',0,0.05f);
    if (slowed.gear == 'N') {
      if (downGear != '1' || slowed.speed >= 0.5f) return fail("neutral before slowing through the gears");
      break;
    }
    if (slowed.gear > downGear || slowed.gear < downGear-1) return fail("downshifts should be sequential");
    if (slowed.gear < downGear) {
      if (downRpm >= 667) return fail("RPM bar should empty before a downshift");
      if (slowed.rpm < 7200) return fail("RPM should return to red on a downshift");
      downGear=slowed.gear;
      ++downshifts;
    } else if (slowed.rpm>downRpm+1) return fail("RPM should fall while slowing in a gear");
    downRpm=slowed.rpm;
  }
  if (downshifts != 4) return fail("did not downshift through all five gears");
  SimulatedDrive coarse;
  auto prior=coarse.update('D',10.4f,0.2f);
  for (int i=0; i<200; ++i) {
    const auto state=coarse.update('D',10.4f,0.2f);
    if (state.gear>prior.gear && (prior.rpm<7900 || state.rpm>2500))
      return fail("upshift should show red then low even after a slow update");
    prior=state;
  }
  int coarseDownshifts=0;
  for (int i=0; i<100; ++i) {
    const auto state=coarse.update('N',0,0.2f);
    if (state.gear=='N') break;
    if (state.gear<prior.gear) {
      if (state.gear!=prior.gear-1 || prior.rpm>=667 || state.rpm<7200)
        return fail("downshift should show empty then red even after a slow update");
      ++coarseDownshifts;
    }
    prior=state;
  }
  if (coarseDownshifts!=4) return fail("slow updates skipped a downshift");
  SimulatedDrive bounded;
  for (int i=0; i<400; ++i) bounded.update('D',100,0.05f);
  if (bounded.update('D',100,0.05f).speed > 10.4f) return fail("crawler speed was not capped");
  for (int i=0; i<100; ++i) bounded.update('D',3,0.05f);
  for (int i=0; i<100; ++i) {
    if (bounded.update('D',3,0.05f).gear != '2') return fail("gear hunts at steady speed");
  }
  for (int i=0; i<400; ++i) drive.update('D',10.4f,0.05f);
  for (int i=0; i<20; ++i) drive.update('D',4,0.05f);
  const auto reverse = drive.update('R',20,0.05f);
  if (reverse.gear != 'R' || reverse.rpm <= 0) return fail("reverse");
  const auto idle = drive.update('N',0,0.05f);
  if (idle.gear != 'N' || idle.rpm != 0 || idle.speed >= reverse.speed) return fail("neutral deceleration");
  puts("PASS: five-speed redline upshifts and sequential downshifts");
}
