#pragma once
#include <stdint.h>
#include <stddef.h>

// Exact RGB565 screen updates. Every client sends the hashes of its own cached
// tiles, so clients never depend on each other's connection or update order.
namespace FrameDelta {
constexpr unsigned WIDTH=240, HEIGHT=135, TILE_WIDTH=16, TILE_HEIGHT=15;
constexpr unsigned COLUMNS=WIDTH/TILE_WIDTH, ROWS=HEIGHT/TILE_HEIGHT;
constexpr unsigned TILE_COUNT=COLUMNS*ROWS, FULL_BYTES=WIDTH*HEIGHT*2;
constexpr unsigned TILE_BYTES=1+TILE_WIDTH*TILE_HEIGHT*2;
struct Plan { bool full; uint8_t count; uint8_t changed[TILE_COUNT]; size_t bytes; };

inline uint32_t hashTile(const uint16_t* pixels, unsigned tile) {
  uint32_t h=2166136261u;
  const unsigned x=(tile%COLUMNS)*TILE_WIDTH, y=(tile/COLUMNS)*TILE_HEIGHT;
  for (unsigned row=0;row<TILE_HEIGHT;++row) for (unsigned col=0;col<TILE_WIDTH;++col) {
    const uint16_t p=pixels[(y+row)*WIDTH+x+col];
    h=(h^uint8_t(p))*16777619u;
    h=(h^uint8_t(p>>8))*16777619u;
  }
  return h;
}
inline bool parseHashes(const char* text, size_t length, uint32_t* hashes) {
  if (!text || !hashes || length!=TILE_COUNT*8) return false;
  for (unsigned i=0;i<TILE_COUNT;++i) {
    uint32_t value=0;
    for (unsigned j=0;j<8;++j) {
      const char c=text[i*8+j];
      const int nibble=c>='0'&&c<='9'?c-'0':(c>='a'&&c<='f'?c-'a'+10:-1);
      if (nibble<0) return false;
      value=(value<<4)|unsigned(nibble);
    }
    hashes[i]=value;
  }
  return true;
}
inline Plan plan(const uint16_t* pixels, const uint32_t* hashes) {
  Plan result{};
  for (unsigned i=0;i<TILE_COUNT;++i)
    if (hashTile(pixels,i)!=hashes[i]) result.changed[result.count++]=uint8_t(i);
  result.bytes=6+size_t(result.count)*TILE_BYTES;
  result.full=result.bytes>=FULL_BYTES+4;
  if (result.full) result.bytes=FULL_BYTES+4;
  return result;
}
template <typename Callback>
inline void forEachDirtyRun(const Plan& plan, Callback callback) {
  bool dirty[TILE_COUNT]{};
  for (unsigned i=0;i<plan.count;++i) dirty[plan.changed[i]]=true;
  for (unsigned row=0;row<ROWS;++row) {
    unsigned col=0;
    while (col<COLUMNS) {
      if (!dirty[row*COLUMNS+col]) { ++col;continue; }
      const unsigned start=col++;
      while (col<COLUMNS && dirty[row*COLUMNS+col]) ++col;
      callback(start*TILE_WIDTH,row*TILE_HEIGHT,(col-start)*TILE_WIDTH,TILE_HEIGHT);
    }
  }
}
inline size_t encode(const uint16_t* pixels, const Plan& plan, uint8_t* out, size_t capacity) {
  if (!pixels || !out || capacity<plan.bytes) return 0;
  out[0]='M';out[1]='D';out[2]=plan.full?'F':'L';out[3]='1';
  size_t offset=4;
  if (plan.full) {
    for (unsigned i=0;i<WIDTH*HEIGHT;++i) { const uint16_t p=pixels[i];out[offset++]=uint8_t(p);out[offset++]=uint8_t(p>>8); }
  } else {
    out[offset++]=plan.count;out[offset++]=0;
    for (unsigned i=0;i<plan.count;++i) {
      const unsigned tile=plan.changed[i],x=(tile%COLUMNS)*TILE_WIDTH,y=(tile/COLUMNS)*TILE_HEIGHT;
      out[offset++]=uint8_t(tile);
      for (unsigned row=0;row<TILE_HEIGHT;++row) for (unsigned col=0;col<TILE_WIDTH;++col) {
        const uint16_t p=pixels[(y+row)*WIDTH+x+col];out[offset++]=uint8_t(p);out[offset++]=uint8_t(p>>8);
      }
    }
  }
  return offset;
}
}
