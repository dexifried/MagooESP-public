#include "DashboardUi.h"
#include "DashboardLayout.h"
#include "PhoneControl.h"
#include "HomeMap.h"
#include "AlbumArt.h"
#include "RobotoFont.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint16_t BLACK=0x0000, WHITE=0xFFFF, MUTED=0xAD75, PANEL=0x1082, BORDER=0x2945;
constexpr float PI_F=3.14159265f;
struct Font { const GlyphInfo* glyphs; const uint8_t* pixels; uint8_t height; };
const Font small{FONT_SMALL_GLYPHS,FONT_SMALL_DATA,FONT_SMALL_HEIGHT};
const Font medium{FONT_MEDIUM_GLYPHS,FONT_MEDIUM_DATA,FONT_MEDIUM_HEIGHT};
const Font digits{FONT_DIGIT_GLYPHS,FONT_DIGIT_DATA,FONT_DIGIT_HEIGHT};

uint16_t blend(uint16_t bg,uint16_t fg,unsigned a) {
  const unsigned inv=255-a;
  const unsigned r=(((bg>>11)&31)*inv+((fg>>11)&31)*a+127)/255;
  const unsigned g=(((bg>>5)&63)*inv+((fg>>5)&63)*a+127)/255;
  const unsigned b=((bg&31)*inv+(fg&31)*a+127)/255;
  return (r<<11)|(g<<5)|b;
}
GlyphInfo glyph(const Font& f, char ch) {
  const unsigned char u=(unsigned char)ch;
  const int index=u>=32 && u<=126 ? u-32 : '?' - 32;
  GlyphInfo result; memcpy_P(&result, f.glyphs+index, sizeof(result)); return result;
}
int textWidth(const Font& f,const char* text) {
  int width=0; for (;*text;++text) width+=glyph(f,*text).advance; return width;
}
void character(GFXcanvas16& c,int x,int y,char ch,const Font& font,uint16_t color) {
  const GlyphInfo g=glyph(font,ch);
  for (int row=0;row<font.height;row++) for (int col=0;col<g.width;col++) {
    const int px=row*g.width+col;
    const uint8_t packed=pgm_read_byte(font.pixels+g.offset+px/2);
    const unsigned a=(px&1 ? packed&15 : packed>>4)*17;
    if (!a || x+col<0 || x+col>=240 || y+row<0 || y+row>=135) continue;
    c.drawPixel(x+col,y+row,a==255?color:blend(c.getPixel(x+col,y+row),color,a));
  }
}
void label(GFXcanvas16& c,int x,int y,const char* text,const Font& font,uint16_t color,int maxWidth=240) {
  const bool truncated=textWidth(font,text)>maxWidth;
  const int dotWidth=textWidth(font,"...");
  const int available=truncated ? maxWidth-dotWidth : maxWidth;
  int used=0;
  while (*text) {
    const int width=glyph(font,*text).advance;
    if (used+width>available) break;
    character(c,x+used,y,*text,font,color); used+=width; ++text;
  }
  if (truncated && dotWidth<=maxWidth) for (int i=0;i<3;i++) {
    character(c,x+used,y,'.',font,color); used+=glyph(font,'.').advance;
  }
}
float clampf(float v,float lo,float hi) { return v<lo?lo:(v>hi?hi:v); }
struct RadialLine { int16_t x1,y1,x2,y2; };
RadialLine radialLine(int cx,int cy,float degree,int r1,int r2) {
  const float a=degree*PI_F/180.0f;const float dx=cosf(a),dy=sinf(a);
  return {int16_t(cx+(int)(dx*r1)),int16_t(cy+(int)(dy*r1)),int16_t(cx+(int)(dx*r2)),int16_t(cy+(int)(dy*r2))};
}
void pointer(GFXcanvas16& c,int x,int y,float bearing,uint16_t accent) {
  const float angle=bearing*PI_F/180.0f,dx=sinf(angle),dy=-cosf(angle);
  c.fillCircle(x,y,8,WHITE);
  c.fillTriangle(x+dx*7,y+dy*7,x-dy*5-dx*4,y+dx*5-dy*4,x+dy*5-dx*4,y-dx*5-dy*4,accent);
}
void mapBody(GFXcanvas16& c,int x,int y,int w,int h,const MapFrame& map,bool live,bool dark,uint16_t accent) {
  const uint16_t bg=dark?0x18E4:0xD69A, road=dark?0x4A49:0xAD55, center=dark?0x73AF:WHITE;
  c.fillRect(x,y,w,h,bg);
  if (live) {
    static uint16_t palette[256];
    static bool paletteReady=false;
    if (!paletteReady) {
      for (unsigned p=0;p<256;p++) palette[p]=((p>>5)*31/7<<11)|(((p>>2)&7)*63/7<<5)|((p&3)*31/3);
      paletteReady=true;
    }
    for (int py=0;py<h;py++) for (int px=0;px<w;px++) {
      const int sx=px*MAP_WIDTH/w, sy=py*MAP_HEIGHT/h;
      const uint8_t p=map.pixels[sy*MAP_WIDTH+sx];
      c.drawPixel(x+px,y+py,palette[p]);
    }
  } else {
    for (const auto& segment : HOME_ROADS) {
      const int x1=x+segment[0]*(w-1)/143,y1=y+segment[1]*(h-1)/103;
      const int x2=x+segment[2]*(w-1)/143,y2=y+segment[3]*(h-1)/103;
      for(int off=-1;off<=1;off++) c.drawLine(x1,constrain(y1+off,y,y+h-1),x2,constrain(y2+off,y,y+h-1),road);
      c.drawLine(x1,y1,x2,y2,center);
    }
  }
  pointer(c,x+w/2,y+h/2,live && map.hasBearing?map.bearing:0.0f,accent);
}
// Remove the pixels outside the rounded card after painting the map and overlays.
void roundMap(GFXcanvas16& c,int x,int y,int w,int h) {
  constexpr int radius=5;
  constexpr uint16_t background=0x0841;
  for(int py=0;py<radius;py++) for(int px=0;px<radius;px++) {
    const int dx=radius-px,dy=radius-py;
    if(dx*dx+dy*dy>radius*radius) {
      c.drawPixel(x+px,y+py,background);
      c.drawPixel(x+w-1-px,y+py,background);
      c.drawPixel(x+px,y+h-1-py,background);
      c.drawPixel(x+w-1-px,y+h-1-py,background);
    }
  }
  c.drawRoundRect(x,y,w,h,radius,0x632C);
}
void attribution(GFXcanvas16& c,int x,int y,int width) {
  c.fillRoundRect(x,y,width,12,3,0x1082);
  label(c,x+3,y+1,"(c) OSM contributors",small,WHITE,width-6);
}
void controls(GFXcanvas16& c,int center,int y,bool playing,uint16_t color,bool filled,int spacing=23) {
  c.drawFastVLine(center-spacing,y-3,7,color);
  c.fillTriangle(center-spacing+1,y,center-spacing+7,y-4,center-spacing+7,y+4,color);
  if (filled) { c.fillCircle(center,y,9,WHITE); color=0x1082; }
  if (playing) { c.fillRect(center-3,y-4,2,8,color); c.fillRect(center+1,y-4,2,8,color); }
  else c.fillTriangle(center-2,y-5,center-2,y+5,center+4,y,color);
  if (filled) color=WHITE;
  c.drawFastVLine(center+spacing,y-3,7,color);
  c.fillTriangle(center+spacing-1,y,center+spacing-7,y-4,center+spacing-7,y+4,color);
}
void mapIcon(GFXcanvas16& c,int x,int y,uint16_t accent) {
  c.fillCircle(x,y,7,accent);c.drawCircle(x,y-2,2,WHITE);c.fillTriangle(x-2,y,x+2,y,x,y+4,WHITE);
}
void musicIcon(GFXcanvas16& c,int x,int y,uint16_t accent) {
  c.fillCircle(x,y,7,accent);c.drawFastVLine(x+1,y-4,7,WHITE);c.drawLine(x+1,y-4,x+4,y-5,WHITE);c.fillCircle(x-1,y+3,2,WHITE);
}
void dock(GFXcanvas16& c,uint16_t accent) {
  c.fillRoundRect(3,110,234,22,6,0x18C3);
  for(int i=0;i<3;i++)for(int j=0;j<3;j++)c.fillCircle(12+i*3,118+j*3,1,MUTED);
  using namespace DashboardLayout;
  mapIcon(c,mapX,121,accent);musicIcon(c,musicX,121,accent);
  c.fillCircle(phoneX,121,7,0x528A);c.drawLine(phoneX-3,118,phoneX-1,123,WHITE);c.drawLine(phoneX-1,123,phoneX+3,125,WHITE);
  c.fillCircle(settingsX,121,7,0x528A);c.drawCircle(settingsX,121,4,WHITE);c.fillCircle(settingsX,121,1,WHITE);
}

void home(GFXcanvas16& c,const Telemetry& d,uint16_t accent) {
  const MapFrame& map=phoneControlMap();const bool live=map.valid && uint32_t(millis()-map.receivedAt)<30000;
  const bool dark=d.homeMapStyle!=0;const uint16_t card=0x18C3, tint=blend(0x18C3,accent,90);
  c.fillScreen(0x0841);mapBody(c,3,3,141,102,map,live,dark,accent);
  c.fillRoundRect(8,8,103,17,8,dark?0x2945:WHITE);
  c.fillCircle(17,16,3,accent);c.fillTriangle(15,18,19,18,17,21,accent);
  label(c,25,11,live?"Phone GPS":"Home map",small,dark?WHITE:0x39C7,81);
  attribution(c,5,92,130);
  roundMap(c,3,3,141,102);
  c.fillRoundRect(149,3,88,39,6,card);
  c.fillCircle(159,16,6,blend(card,accent,105));
  c.drawLine(156,16,159,13,WHITE);c.drawLine(159,13,162,16,WHITE);c.drawRect(157,16,5,4,WHITE);
  label(c,169,10,"Home",medium,WHITE,62);
  label(c,169,23,live?"Phone GPS":"Offline",small,MUTED,62);
  c.fillRoundRect(149,46,88,59,6,tint);
  label(c,155,48,d.trackTitle,medium,WHITE,76);
  label(c,155,62,d.trackArtist,small,0xE71C,76);
  c.drawFastHLine(155,77,76,blend(tint,WHITE,75));
  if(d.trackDuration)c.drawFastHLine(155,77,clampf((float)d.trackPosition/d.trackDuration,0,1)*76,WHITE);
  controls(c,192,92,d.trackPlaying,WHITE,false);
  dock(c,accent);
}
void music(GFXcanvas16& c,const Telemetry& d,uint16_t accent) {
  const uint16_t card=0x1082; c.fillScreen(0x0841);c.fillRoundRect(3,3,234,129,8,card);c.drawRoundRect(3,3,234,129,8,BORDER);
  label(c,11,7,"NOW PLAYING",small,MUTED,150);musicIcon(c,216,14,accent);
  const uint16_t* art=d.trackArt==1?ART_LOVE:(d.trackArt==2?ART_GOODBYE:ART_EIGHT);
  c.drawRGBBitmap(10,27,art,60,60);
  label(c,80,30,d.trackTitle,medium,WHITE,148);label(c,80,47,d.trackArtist,small,MUTED,148);
  label(c,80,64,"Preview - no audio",small,blend(MUTED,WHITE,25),145);
  c.fillRoundRect(10,93,218,3,1,0x39E7);
  const int filled=d.trackDuration?(int)(clampf((float)d.trackPosition/d.trackDuration,0,1)*218):0;
  if(filled>0)c.fillRect(10,93,filled,3,accent);
  char elapsed[8],duration[8];snprintf(elapsed,sizeof(elapsed),"%u:%02u",d.trackPosition/60,d.trackPosition%60);snprintf(duration,sizeof(duration),"%u:%02u",d.trackDuration/60,d.trackDuration%60);
  label(c,10,108,elapsed,small,MUTED,37);label(c,205,108,duration,small,MUTED,28);
  controls(c,120,116,d.trackPlaying,WHITE,true,50);
}
void navigation(GFXcanvas16& c,const Telemetry& d,uint16_t accent) {
  const MapFrame& map=phoneControlMap();const bool live=map.valid && uint32_t(millis()-map.receivedAt)<30000;const bool dark=d.homeMapStyle!=0;
  c.fillScreen(0x0841);mapBody(c,3,3,142,129,map,live,dark,accent);
  c.fillRoundRect(8,8,113,18,8,dark?0x2945:WHITE);c.fillCircle(17,17,3,accent);c.fillTriangle(15,18,19,18,17,21,accent);
  label(c,25,11,live?"Phone GPS":"Home / offline",small,dark?WHITE:0x39C7,92);
  attribution(c,5,117,130);
  roundMap(c,3,3,142,129);
  c.fillRoundRect(150,3,87,129,7,0x18C3);c.drawRoundRect(150,3,87,129,7,BORDER);
  label(c,158,10,d.demo?"DEMO SPEED":"THROTTLE",small,MUTED,72);
  char speed[8];const float navSpeed=clampf(d.speedKph,0,100);
  if(navSpeed>=99.95f)snprintf(speed,sizeof(speed),"100");else snprintf(speed,sizeof(speed),"%.1f",navSpeed);
  const int sw=textWidth(digits,speed);label(c,193-sw/2,28,speed,digits,WHITE,78);
  label(c,181,70,"km/h",small,MUTED,45);
  c.drawFastHLine(159,85,68,blend(0x18C3,WHITE,35));
  char navBattery[16];
  if (d.voltageAvailable) snprintf(navBattery,sizeof(navBattery),"BAT  %.1fV",d.voltage);
  else snprintf(navBattery,sizeof(navBattery),"BAT  --.-V");
  label(c,159,90,navBattery,small,MUTED,70);label(c,159,108,d.demo?"RPM":"RPM EST",small,MUTED,70);
}
// Downsample the existing anti-aliased display numerals for compact readings.
int inclineNumberWidth(const char* text) {
  int width=0;
  while (*text) {
    const char ch=*text++;
    width += ch=='-' ? 9 : (glyph(digits,ch).advance*3+2)/5;
  }
  return width;
}
void inclineNumber(GFXcanvas16& c,int x,int y,const char* text,uint16_t color) {
  while (*text) {
    const char ch=*text++;
    if (ch=='-') { c.fillRoundRect(x,y+13,6,2,1,color); x+=9; continue; }
    const GlyphInfo g=glyph(digits,ch);
    const int width=(g.width*3+2)/5, height=(digits.height*3+2)/5;
    for (int dy=0;dy<height;dy++) for (int dx=0;dx<width;dx++) {
      const int sx=dx*5/3, sy=dy*5/3;
      if (sx>=g.width || sy>=digits.height) continue;
      const int pixel=sy*g.width+sx;
      const uint8_t packed=pgm_read_byte(digits.pixels+g.offset+pixel/2);
      const unsigned alpha=((pixel&1)?(packed&15):(packed>>4))*17;
      if (alpha) c.drawPixel(x+dx,y+dy,alpha==255?color:blend(c.getPixel(x+dx,y+dy),color,alpha));
    }
    x+=(g.advance*3+2)/5;
  }
}
void inclineVehicle(GFXcanvas16& c,int cx,int cy,float degrees,bool front,uint16_t accent) {
  const float radians=-degrees*PI_F/180.0f,cs=cosf(radians),sn=sinf(radians);
  // Keep a constant size at every angle so the vehicle only rotates.
  const float scale=front ? 1.0f : 1.05f;
  auto px=[&](float x,float y) { return cx+int(roundf((x*cs-y*sn)*scale)); };
  auto py=[&](float x,float y) { return cy+int(roundf((x*sn+y*cs)*scale)); };
  auto line=[&](float x1,float y1,float x2,float y2,uint16_t color) {
    c.drawLine(px(x1,y1),py(x1,y1),px(x2,y2),py(x2,y2),color);
  };
  if(front) {
    // Tires sit behind the body, so they cannot obscure the headlights.
    for(int x : {-15,15}) {
      const int ax=px(x-2.5f,4), ay=py(x-2.5f,4), bx=px(x+2.5f,4), by=py(x+2.5f,4);
      const int tx=px(x+2.5f,12), ty=py(x+2.5f,12), dx=px(x-2.5f,12), dy=py(x-2.5f,12);
      const uint16_t rubber=0x3186, edge=0x7BEF;
      c.fillTriangle(ax,ay,bx,by,tx,ty,rubber);
      c.fillTriangle(ax,ay,tx,ty,dx,dy,rubber);
      c.drawLine(ax,ay,bx,by,edge); c.drawLine(bx,by,tx,ty,edge);
      c.drawLine(tx,ty,dx,dy,edge); c.drawLine(dx,dy,ax,ay,edge);
      line(x-1,7,x+1,7,edge); line(x-1,10,x+1,10,edge);
    }
    // Flat windshield, upright body, round headlights, and the Jeep-style grille.
    line(-12,-5,-11,-13,WHITE); line(-11,-13,11,-13,WHITE);
    line(11,-13,12,-5,WHITE); line(-12,-5,12,-5,WHITE);
    line(0,-12,0,-6,blend(PANEL,WHITE,110));
    line(-18,-4,-18,5,WHITE); line(-18,-4,18,-4,WHITE);
    line(18,-4,18,5,WHITE); line(-18,5,18,5,WHITE);
    line(-19,5,19,5,accent);
    c.fillCircle(px(-13,0),py(-13,0),3,WHITE);
    c.fillCircle(px(13,0),py(13,0),3,WHITE);
    c.fillCircle(px(-13,0),py(-13,0),1,accent);
    c.fillCircle(px(13,0),py(13,0),1,accent);
    for(int x=-6;x<=6;x+=2) line(x,-1,x,3,WHITE);
  } else {
    // Upright cabin and windshield, flat hood, and a rear-mounted spare.
    line(-17,5,-17,-5,WHITE); line(-17,-5,6,-5,WHITE);
    line(-14,-5,-14,-13,WHITE); line(-14,-13,4,-13,WHITE);
    line(4,-13,7,-5,WHITE); line(7,-5,19,-5,WHITE);
    line(19,-5,19,5,WHITE); line(-17,5,19,5,WHITE);
    line(-6,-12,-6,-5,blend(PANEL,WHITE,150));
    line(-6,-4,-6,4,blend(PANEL,WHITE,110));
    line(7,-4,7,4,blend(PANEL,WHITE,110));
    line(9,-4,18,-4,accent);
    c.fillCircle(px(-20,-1),py(-20,-1),3,PANEL);
    c.drawCircle(px(-20,-1),py(-20,-1),3,WHITE);
    c.fillCircle(px(-20,-1),py(-20,-1),1,accent);
  }
  if(!front) {
    for(int x : {-12,12}) {
      c.fillCircle(px(x,7),py(x,7),5,PANEL);
      c.drawCircle(px(x,7),py(x,7),4,WHITE);
      c.fillCircle(px(x,7),py(x,7),1,accent);
    }
  }
}
void inclineCard(GFXcanvas16& c,int x,const char* title,float angle,bool valid,bool front,uint16_t accent) {
  constexpr int y=24,w=110,h=104;
  c.fillRoundRect(x,y,w,h,7,PANEL);
  c.drawRoundRect(x,y,w,h,7,BORDER);
  label(c,x+(w-textWidth(small,title))/2,y+5,title,small,MUTED,80);
  if(valid) {
    char value[8]; snprintf(value,sizeof(value),"%ld",long(lroundf(angle)));
    const int valueX=x+(w-inclineNumberWidth(value)-6)/2;
    inclineNumber(c,valueX,97,value,WHITE);
    c.drawCircle(valueX+inclineNumberWidth(value)+3,104,2,accent);
    const int centerX=x+w/2, centerY=67;
    c.drawFastHLine(centerX-35,centerY+17,71,blend(PANEL,WHITE,35));
    inclineVehicle(c,centerX,centerY,angle,front,accent);
  } else {
    label(c,x+(w-textWidth(medium,"--"))/2,65,"--",medium,MUTED,80);
    label(c,x+(w-textWidth(small,"NO SENSOR"))/2,103,"NO SENSOR",small,MUTED,80);
  }
}
void tiltView(GFXcanvas16& c,const Telemetry& d,uint16_t accent) {
  c.fillScreen(0x0841);
  c.fillCircle(11,11,2,accent);
  label(c,18,5,"TRAIL / INCLINE",small,WHITE,145);
  const char* status=d.tiltAvailable?"LIVE":"OFFLINE";
  label(c,230-textWidth(small,status),5,status,small,d.tiltAvailable?accent:MUTED,50);
  inclineCard(c,7,"PITCH",d.pitch,d.tiltAvailable,false,accent);
  inclineCard(c,123,"ROLL",d.roll,d.tiltAvailable,true,accent);
}
void speedometer(GFXcanvas16& c,const Telemetry& d,uint16_t accent,uint16_t dimAccent) {
  c.fillScreen(0x0841);c.fillRoundRect(2,2,236,131,9,0x0841);c.drawRoundRect(2,2,236,131,9,BORDER);
  c.fillCircle(13,13,2,accent);label(c,19,7,"MAGOO / RC",small,MUTED,100);
  const char* status=d.demo?"NO SIGNAL":"THROTTLE";const int statusWidth=textWidth(small,status);label(c,227-statusWidth,7,status,small,d.demo?MUTED:accent,65);
  c.fillRoundRect(7,42,32,39,6,PANEL); c.drawRoundRect(7,42,32,39,6,BORDER);
  label(c,23-textWidth(small,"GEAR")/2,45,"GEAR",small,MUTED,28);
  const char gearText[2]={d.driveDirection,0};
  label(c,23-textWidth(medium,gearText)/2,60,gearText,medium,d.driveDirection=='-'?MUTED:accent,28);
  static RadialLine dots[71],ticks[17];
  static bool geometryReady=false;
  if (!geometryReady) {
    for(int i=0;i<=70;i++) dots[i]=radialLine(120,91,200.0f+i*2.0f,77,79);
    for(int i=0;i<=16;i++) ticks[i]=radialLine(120,91,200.0f+i*140.0f/16.0f,i%4==0?67:72,78);
    geometryReady=true;
  }
  // Highlight the travelled part of the dial without changing the layout.
  const float speed=clampf(d.speedKph,0,100);
  const uint16_t dotColor=blend(0x0841,accent,65);
  const int litDots=d.speedAvailable ? int(speed*70.0f/100.0f) : 0;
  for(int i=0;i<=70;i++) { const auto& line=dots[i]; c.drawLine(line.x1,line.y1,line.x2,line.y2,i<litDots?accent:dotColor); }
  for(int i=0;i<=16;i++) { const auto& line=ticks[i]; c.drawLine(line.x1,line.y1,line.x2,line.y2,i%4==0?accent:0x73AE); }
  const float angle=(200.0f+speed*1.4f)*PI_F/180.0f,dx=cosf(angle),dy=sinf(angle);
  // Keep the pointer on the dial so it does not cross the speed digits.
  const int bx=120+(int)(dx*66),by=91+(int)(dy*66),tx=120+(int)(dx*76),ty=91+(int)(dy*76);
  if(d.speedAvailable) {c.fillTriangle(bx+(int)(-dy*4),by+(int)(dx*4),bx-(int)(-dy*4),by-(int)(dx*4),tx,ty,accent);c.drawLine(bx,by,tx,ty,WHITE);}
  char value[8];if(!d.speedAvailable)snprintf(value,sizeof(value),"0.0");else if(speed>=99.95f)snprintf(value,sizeof(value),"100");else snprintf(value,sizeof(value),"%.1f",speed);
  const int width=textWidth(digits,value);label(c,120-width/2,29,value,digits,WHITE,80);
  const char* unit=d.demo?"km/h":"EST  km/h";label(c,120-textWidth(small,unit)/2,73,unit,small,MUTED,90);
  label(c,53,65,"0",small,MUTED,10);label(c,176,65,"100",small,MUTED,22);
  c.drawFastHLine(11,100,218,BORDER);
  label(c,11,102,d.demo?"RPM":"RPM EST",small,MUTED,80);
  label(c,195,102,"BAT",small,MUTED,38);
  const int active=(int)(clampf(d.rpm,0,8000)*12/8000);
  // Warn as the simulated RPM approaches the 8,000 RPM shift point.
  const uint16_t rpmColor=d.rpm>=7200.0f?0xF800:(d.rpm>=6000.0f?0xFFE0:WHITE);
  for(int i=0;i<12;i++)c.fillRoundRect(11+i*6,119,5,7,1,d.rpmAvailable && i<active?rpmColor:0x2104);
  c.drawFastVLine(84,118,9,accent);
  c.drawRoundRect(174,118,14,9,2,MUTED);c.fillRect(188,121,2,3,MUTED);
  c.fillRoundRect(176,120,10,5,1,WHITE);
  label(c,195,118,"12.6V",small,WHITE,38);
  if (d.tiltAvailable) {
    // Rotate a fixed-length bar around its center, as in the incline view.
    const float pitchAngle=-clampf(d.pitch,-90.0f,90.0f)*PI_F/180.0f;
    const int dx=(int)roundf(14.0f*cosf(pitchAngle));
    const int dy=(int)roundf(14.0f*sinf(pitchAngle));
    c.drawLine(120-dx,118-dy,120+dx,118+dy,accent);
    c.fillCircle(120-dx,118-dy,1,accent);
    c.fillCircle(120+dx,118+dy,1,accent);
    c.fillCircle(120,118,2,accent);
    char pitchText[8];
    snprintf(pitchText,sizeof(pitchText),"%ld",long(lroundf(d.pitch)));
    label(c,138,114,pitchText,small,WHITE,27);
    c.drawCircle(138+textWidth(small,pitchText)+2,116,1,accent);
  } else {
    c.drawFastHLine(106,118,29,dimAccent);
    label(c,138,114,"--",small,MUTED,27);
  }
}
}

namespace DashboardUi {
void render(GFXcanvas16& c,const Telemetry& data) {
  const uint16_t accents[]={0xF800,0x05FF,0xFD20,0x07E0};
  const uint16_t dark[]={0x7800,0x02AA,0x8200,0x03A0};
  const unsigned index=data.colorTheme<4?data.colorTheme:0;
  if(data.theme==7)tiltView(c,data,accents[index]);
  else if(data.theme==6)home(c,data,accents[index]);
  else if(data.theme==5)music(c,data,accents[index]);
  else if(data.theme==4)navigation(c,data,accents[index]);
  else speedometer(c,data,accents[index],dark[index]);
}
}
