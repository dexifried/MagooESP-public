#include "WifiPairing.h"
#include <assert.h>
#include <stdio.h>

int main() {
  WifiPairing pairing;
  assert(pairing.slot() == -1);
  assert(pairing.state() == WifiPairing::Idle);
  pairing.start(1, 123);
  assert(pairing.id() == 123);
  assert(pairing.slot() == 1 && pairing.state() == WifiPairing::Connecting);
  pairing.finish(0, true);
  assert(pairing.state() == WifiPairing::Connecting);
  pairing.finish(1, false);
  assert(pairing.state() == WifiPairing::Failed);
  pairing.finish(1, true); // Automatic fallback must not turn failure into success.
  assert(pairing.state() == WifiPairing::Failed);
  pairing.start(0, 456);
  assert(pairing.id() == 456);
  pairing.finish(0, true);
  assert(pairing.slot() == 0 && pairing.state() == WifiPairing::Connected);
  puts("PASS: pairing reports the requested attempt, not the fallback connection");
}
