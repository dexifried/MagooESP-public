#include "MusicPackage.h"
#include <esp_partition.h>
#include <string.h>

namespace MusicPackage {
namespace {
constexpr size_t Sector=4096, SlotSize=65536, SlotHeader=16, Reserved=Sector+2*SlotSize;
constexpr uint8_t StoreMagic[8]={'M','A','G','S','T','O','R','1'};
constexpr uint8_t SlotMagic[8]={'M','A','G','S','L','O','T','1'};
const esp_partition_t* part=nullptr;
size_t base=0; int active=-1, target=-1; uint32_t sequence=0,tokenValue=0,lastWrite=0;
size_t expected=0,written=0; bool installed=false,installing=false,cooldown=false;
uint32_t cooldownUntil=0;
const char* lastError="Storage unavailable";
uint32_t jpgSizes[3]={},savedChecksum=0; Layout savedLayout{};
uint32_t le32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t le16(const uint8_t* p) {return uint16_t(p[0])|uint16_t(p[1])<<8;}
void put32(uint8_t* p,uint32_t n) {for(int i=0;i<4;i++)p[i]=uint8_t(n>>(i*8));}
size_t slotBase(int slot) {return base+Sector+size_t(slot)*SlotSize;}
bool read(size_t off,void* data,size_t n) {return part && off+n<=part->size && esp_partition_read(part,off,data,n)==ESP_OK;}
uint32_t crc32(uint32_t crc,const uint8_t* data,size_t size) {
  while(size--) {crc^=*data++;for(int i=0;i<8;i++)crc=(crc>>1)^(0xedb88320UL & (0U-(crc&1U)));} return crc;
}
bool allBlank(size_t off,size_t size) {
  uint8_t b[512];while(size){size_t n=min(sizeof(b),size);if(!read(off,b,n))return false;for(size_t i=0;i<n;i++)if(b[i]!=0xff)return false;off+=n;size-=n;}return true;
}
bool storeOwned() {uint8_t marker[8];return read(base,marker,sizeof(marker)) && !memcmp(marker,StoreMagic,sizeof(marker));}
bool validPackage(int slot,size_t len,bool remember) {
  uint8_t h[HeaderSize],l[LayoutSize]; const size_t start=slotBase(slot)+SlotHeader;
  if(len<HeaderSize+LayoutSize+3*RawSize || len>MaxSize || !read(start,h,sizeof(h)) ||
     memcmp(h,"MGMP",4) || le32(h+4)!=1 || le32(h+8)!=len || le32(h+28)!=RawSize ||
     uint64_t(HeaderSize+LayoutSize+3*RawSize)+le32(h+16)+le32(h+20)+le32(h+24)!=len ||
     !read(start+HeaderSize,l,sizeof(l)))return false;
  Layout v{le16(l),le16(l+2),le16(l+4),le16(l+6),le16(l+8),le16(l+10),le16(l+12),le16(l+14)};
  if(v.artW!=60 || v.artH!=60 || v.artX+60>240 || v.artY+60>135 || v.titleX>=240 || v.titleY>=135 || v.artistX>=240 || v.artistY>=135)return false;
  uint32_t crc=0xffffffffUL;uint8_t buf[512];size_t off=HeaderSize;
  while(off<len){size_t n=min(sizeof(buf),len-off);if(!read(start+off,buf,n))return false;crc=crc32(crc,buf,n);off+=n;}
  if((crc^0xffffffffUL)!=le32(h+12))return false;
  if(remember){savedLayout=v;savedChecksum=le32(h+12);for(int i=0;i<3;i++)jpgSizes[i]=le32(h+16+i*4);}return true;
}
bool validSlot(int slot,uint32_t& seq) {
  uint8_t h[SlotHeader];if(!read(slotBase(slot),h,sizeof(h))||memcmp(h,SlotMagic,8))return false;
  seq=le32(h+8);return validPackage(slot,le32(h+12),false);
}
void cancel(){installing=false;target=-1;tokenValue=0;expected=written=0;cooldown=true;cooldownUntil=millis()+1000;}
int nibble(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
}
void begin(){
  part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!part||part->size<Reserved)return;base=0;
  if(!storeOwned()){lastError="Music not installed";return;}
  uint32_t a=0,b=0;bool va=validSlot(0,a),vb=validSlot(1,b);
  if(va||vb){active=vb&&(!va||int32_t(b-a)>0)?1:0;sequence=active?b:a;uint8_t h[SlotHeader];read(slotBase(active),h,sizeof(h));installed=validPackage(active,le32(h+12),true);}
  lastError=installed?"":"Music not installed";
}
bool ready(){return installed;} bool busy(){if(cooldown && int32_t(millis()-cooldownUntil)>=0)cooldown=false;return installing||cooldown;} uint32_t checksum(){return savedChecksum;} const char* error(){return lastError;}
bool start(size_t total,uint32_t& token){
  if(!part){lastError="No compatible storage partition";return false;}
  if(busy()||total<HeaderSize+LayoutSize+3*RawSize||total>MaxSize){lastError="Invalid package size or installation already active";return false;}
  // Claim only a completely blank bounded region. Other bytes are never written.
  // No filesystem may use this partition concurrently.
  if(!storeOwned()) {
    if(!allBlank(base,Reserved)){lastError="The reserved storage area contains data; installation was not started";return false;}
    if(esp_partition_write(part,base,StoreMagic,sizeof(StoreMagic))!=ESP_OK){lastError="Could not reserve blank storage";return false;}
  }
  target=active==0?1:0;installing=true;lastWrite=millis();
  for(size_t i=0;i<SlotSize;i+=Sector){
    if(esp_partition_erase_range(part,slotBase(target)+i,Sector)!=ESP_OK){lastError="Storage erase failed";cancel();return false;}
    yield();
  }
  expected=total;written=0;tokenValue=esp_random();if(!tokenValue)tokenValue=1;token=tokenValue;lastWrite=millis();lastError="";return true;
}
bool append(uint32_t token,size_t offset,const String& hex){
  if(!installing||token!=tokenValue||offset!=written||!hex.length()||hex.length()>2048||(hex.length()&1)||written+hex.length()/2>expected||
     ((hex.length()/2)%4 && written+hex.length()/2!=expected)){lastError="Invalid or out-of-order package chunk";return false;}
  uint8_t bytes[1024];size_t count=hex.length()/2;
  for(size_t i=0;i<count;i++){int a=nibble(hex[i*2]),b=nibble(hex[i*2+1]);if(a<0||b<0){lastError="Invalid package data";return false;}bytes[i]=uint8_t((a<<4)|b);}
  size_t padded=(count+3)&~size_t(3);for(size_t i=count;i<padded;i++)bytes[i]=0xff;
  if(esp_partition_write(part,slotBase(target)+SlotHeader+written,bytes,padded)!=ESP_OK){lastError="Storage write failed";cancel();return false;}
  written+=count;lastWrite=millis();return true;
}
bool finish(uint32_t token){
  if(!installing||token!=tokenValue||written!=expected){lastError="Incomplete package";return false;}
  if(!validPackage(target,expected,false)){lastError="Package validation failed";cancel();return false;}
  uint8_t h[SlotHeader];memcpy(h,SlotMagic,8);put32(h+8,sequence+1);put32(h+12,expected);
  if(esp_partition_write(part,slotBase(target),h,sizeof(h))!=ESP_OK){lastError="Could not complete installation";cancel();return false;}
  active=target;sequence++;installed=validPackage(active,expected,true);cancel();lastError=installed?"":"Installed package could not be read";return installed;
}
void expire(){if(installing&&uint32_t(millis()-lastWrite)>30000){lastError="Installation timed out";cancel();}}
bool layout(Layout& value){if(!installed)return false;value=savedLayout;return true;}
bool readRow(unsigned art,unsigned row,uint16_t* pixels){return installed&&art<3&&row<60&&read(slotBase(active)+SlotHeader+HeaderSize+LayoutSize+art*RawSize+row*120,pixels,120);}
size_t jpegSize(unsigned art){return installed&&art<3?jpgSizes[art]:0;}
bool readJpeg(unsigned art,size_t off,uint8_t* data,size_t count){
  if(!installed||art>=3||off>jpgSizes[art]||count>jpgSizes[art]-off)return false;
  size_t pos=slotBase(active)+SlotHeader+HeaderSize+LayoutSize+3*RawSize;for(unsigned i=0;i<art;i++)pos+=jpgSizes[i];return read(pos+off,data,count);
}
}
