#pragma once
#include <Arduino.h>
namespace MusicPackage {
constexpr size_t HeaderSize=32, LayoutSize=16, RawSize=7200, MaxSize=65520;
struct Layout { uint16_t artX,artY,artW,artH,titleX,titleY,artistX,artistY; };
void begin(); bool ready(); bool busy(); uint32_t checksum(); const char* error();
bool start(size_t total,uint32_t& token);
bool append(uint32_t token,size_t offset,const String& hex);
bool finish(uint32_t token); void expire();
bool layout(Layout& value);
bool readRow(unsigned art,unsigned row,uint16_t* pixels);
size_t jpegSize(unsigned art);
bool readJpeg(unsigned art,size_t offset,uint8_t* data,size_t count);
}
