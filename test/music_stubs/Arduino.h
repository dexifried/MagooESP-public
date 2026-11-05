#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
using std::min;
using String=std::string;
extern uint32_t fakeMillis;
inline uint32_t millis(){return fakeMillis;}
inline uint32_t esp_random(){return 12345;}
inline void yield(){}
