#pragma once
#include <stdint.h>
struct FakeSerial { template <typename... Args> void printf(const char*, Args...) {} void println(const char*) {} };
extern FakeSerial Serial;
inline int digitalRead(int) { return 1; }
