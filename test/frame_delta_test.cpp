#include "FrameDelta.h"
#include <stdio.h>

static uint16_t frame[FrameDelta::WIDTH*FrameDelta::HEIGHT];
static uint32_t hashes[FrameDelta::TILE_COUNT];
static uint8_t encoded[FrameDelta::FULL_BYTES+4];
static uint16_t displayPixels[FrameDelta::WIDTH*FrameDelta::HEIGHT];
static char text[FrameDelta::TILE_COUNT*8+1];

int main() {
  using namespace FrameDelta;
  for (unsigned i=0;i<WIDTH*HEIGHT;++i) frame[i]=uint16_t(i*17);
  for (unsigned i=0;i<WIDTH*HEIGHT;++i) displayPixels[i]=frame[i];
  for (unsigned i=0;i<TILE_COUNT;++i) hashes[i]=hashTile(frame,i);
  if (hashes[0]!=0x4c4428c8u) return 13;
  const char* hex="0123456789abcdef";
  for (unsigned i=0;i<TILE_COUNT;++i) for (int j=0;j<8;++j) text[i*8+j]=hex[(hashes[i]>>(28-j*4))&15];
  uint32_t parsed[TILE_COUNT];
  if (!parseHashes(text,TILE_COUNT*8,parsed)) return 1;
  for (unsigned i=0;i<TILE_COUNT;++i) if (parsed[i]!=hashes[i]) return 2;
  text[7]='x'; if (parseHashes(text,TILE_COUNT*8,parsed)) return 3;
  if (parseHashes(text,TILE_COUNT*8-1,parsed)) return 4;
  auto unchanged=plan(frame,hashes);
  if (unchanged.full || unchanged.count!=0 || unchanged.bytes!=6) return 5;
  if (encode(frame,unchanged,encoded,sizeof(encoded))!=6 || encoded[0]!='M' || encoded[3]!='1') return 6;
  frame[0]^=0xffff;frame[WIDTH*HEIGHT-1]^=0xffff;
  auto partial=plan(frame,hashes);
  if (partial.full || partial.count!=2 || partial.bytes!=6+2*TILE_BYTES) return 7;
  if (encode(frame,partial,encoded,partial.bytes-1)!=0) return 8;
  if (encode(frame,partial,encoded,sizeof(encoded))!=partial.bytes || encoded[6]!=0 || encoded[6+TILE_BYTES]!=TILE_COUNT-1) return 9;
  if (encoded[7]!=uint8_t(frame[0]) || encoded[8]!=uint8_t(frame[0]>>8)) return 10;
  unsigned runs=0;
  forEachDirtyRun(partial,[&](unsigned x,unsigned y,unsigned width,unsigned height) {
    ++runs;
    for (unsigned row=0;row<height;++row) for (unsigned col=0;col<width;++col)
      displayPixels[(y+row)*WIDTH+x+col]=frame[(y+row)*WIDTH+x+col];
  });
  if (runs!=2) return 14;
  for (unsigned i=0;i<WIDTH*HEIGHT;++i) if (displayPixels[i]!=frame[i]) return 15;
  for (unsigned i=0;i<TILE_COUNT;++i) hashes[i]=hashTile(frame,i);
  frame[16]^=1;frame[32]^=1;frame[15*WIDTH]^=1;
  auto grouped=plan(frame,hashes);runs=0;
  if (grouped.count!=3) return 16;
  forEachDirtyRun(grouped,[&](unsigned x,unsigned y,unsigned width,unsigned height) {
    ++runs;
    for (unsigned row=0;row<height;++row) for (unsigned col=0;col<width;++col)
      displayPixels[(y+row)*WIDTH+x+col]=frame[(y+row)*WIDTH+x+col];
  });
  if (runs!=2) return 17;
  for (unsigned i=0;i<WIDTH*HEIGHT;++i) if (displayPixels[i]!=frame[i]) return 18;
  for (unsigned i=0;i<TILE_COUNT;++i) hashes[i]=hashTile(frame,i);
  uint32_t random=1234567;
  for (unsigned round=0;round<60;++round) {
    for (unsigned j=0;j<(round%30);++j) {
      random=random*1664525u+1013904223u;
      const unsigned tile=random%TILE_COUNT;
      const unsigned x=(tile%COLUMNS)*TILE_WIDTH,y=(tile/COLUMNS)*TILE_HEIGHT;
      frame[y*WIDTH+x]^=uint16_t(random|1);
    }
    const auto changed=plan(frame,hashes);
    if (changed.count>=75) for (unsigned i=0;i<WIDTH*HEIGHT;++i) displayPixels[i]=frame[i];
    else forEachDirtyRun(changed,[&](unsigned x,unsigned y,unsigned width,unsigned height) {
      if (x+width>WIDTH || y+height>HEIGHT) { runs=999;return; }
      for (unsigned row=0;row<height;++row) for (unsigned col=0;col<width;++col)
        displayPixels[(y+row)*WIDTH+x+col]=frame[(y+row)*WIDTH+x+col];
    });
    if (runs==999) return 19;
    for (unsigned i=0;i<WIDTH*HEIGHT;++i) if (displayPixels[i]!=frame[i]) return 20;
    for (unsigned i=0;i<changed.count;++i) hashes[changed.changed[i]]=hashTile(frame,changed.changed[i]);
  }
  for (unsigned i=0;i<WIDTH*HEIGHT;++i) frame[i]=uint16_t(i*17)^0x00ff;
  auto full=plan(frame,hashes);
  if (!full.full || full.bytes!=FULL_BYTES+4) return 11;
  if (encode(frame,full,encoded,sizeof(encoded))!=full.bytes || encoded[2]!='F') return 12;
  puts("PASS: frame cache hashes, sparse updates and full resynchronization");
}
