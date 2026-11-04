#include "DisplayChanges.h"
#include <stdio.h>
int main() {
  DisplayChanges tracker;Telemetry t;
  if (!tracker.changed(t,0,false)) return 1;
  tracker.record(t,0,false);
  if (tracker.changed(t,1,true)) return 2; // Map updates do not change speed view.
  t.roll=22;
  if (tracker.changed(t,1,true)) return 3;
  t.pitch=10;
  if (!tracker.changed(t,1,true)) return 4;
  tracker.record(t,1,true);
  t.theme=7;
  if (!tracker.changed(t,1,true)) return 5;
  tracker.record(t,1,true);
  t.roll=23;
  if (!tracker.changed(t,1,true)) return 6;
  t.theme=6;tracker.record(t,1,true);
  if (!tracker.changed(t,1,false) || !tracker.changed(t,2,true)) return 7;
  t.theme=5;tracker.record(t,2,true);
  t.trackPosition++;
  if (!tracker.changed(t,2,true)) return 8;
  tracker.record(t,2,true);
  tracker.invalidate();
  if (!tracker.changed(t,2,true)) return 9;
  puts("PASS: view-specific redraws, map expiry and invalidation");
}
