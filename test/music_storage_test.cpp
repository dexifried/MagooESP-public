#include "MusicPackage.h"
#include <esp_partition.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
uint32_t fakeMillis=0;
static esp_partition_t partition{1408*1024};
static std::vector<uint8_t> flash(partition.size,0xff);
const esp_partition_t* esp_partition_find_first(int,int,const char*){return &partition;}
int esp_partition_read(const esp_partition_t*,size_t off,void* data,size_t n){if(off+n>flash.size())return 1;memcpy(data,&flash[off],n);return ESP_OK;}
int esp_partition_write(const esp_partition_t*,size_t off,const void* data,size_t n){
  if(off+n>flash.size())return 1;const auto* p=static_cast<const uint8_t*>(data);
  for(size_t i=0;i<n;i++){if((flash[off+i]&p[i])!=p[i])return 1;flash[off+i]&=p[i];}return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t*,size_t off,size_t n){
  if(off+n>flash.size()||off%4096||n%4096)return 1;std::fill(flash.begin()+off,flash.begin()+off+n,0xff);return ESP_OK;
}
static std::string hex(const std::vector<uint8_t>& v,size_t off,size_t n){
  static const char* digits="0123456789abcdef";std::string out;
  for(size_t i=off;i<off+n;i++){out+=digits[v[i]>>4];out+=digits[v[i]&15];}return out;
}
static void upload(const std::vector<uint8_t>& data){
  uint32_t token=0;assert(MusicPackage::start(data.size(),token));assert(MusicPackage::busy());
  for(size_t off=0;off<data.size();off+=1024){size_t n=std::min<size_t>(1024,data.size()-off);assert(MusicPackage::append(token,off,hex(data,off,n)));}
  assert(MusicPackage::finish(token));fakeMillis+=1000;assert(!MusicPackage::busy());
}
int main(int argc,char** argv){
  assert(argc==2);std::ifstream in(argv[1],std::ios::binary);assert(in);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),{});assert(!data.empty());
  MusicPackage::begin();assert(!MusicPackage::ready());
  flash[0]=0x12;uint32_t t=0;auto before=flash;assert(!MusicPackage::start(data.size(),t));assert(flash==before);
  flash[0]=0xff;flash.back()=0x42;upload(data);assert(MusicPackage::ready());assert(flash.back()==0x42);
  MusicPackage::Layout layout{};assert(MusicPackage::layout(layout));assert(layout.artX==10&&layout.artY==27&&layout.artW==60);
  uint16_t row[60];assert(MusicPackage::readRow(0,0,row));assert(!memcmp(row,data.data()+48,120));
  assert(MusicPackage::jpegSize(0)>0);std::vector<uint8_t> jpg(MusicPackage::jpegSize(0));assert(MusicPackage::readJpeg(0,0,jpg.data(),jpg.size()));assert(jpg[0]==0xff&&jpg[1]==0xd8);
  // A failed package must not replace the working installation.
  auto corrupt=data;corrupt[100]^=1;assert(MusicPackage::start(corrupt.size(),t));
  assert(!MusicPackage::append(t,1,hex(corrupt,0,1024)));
  for(size_t off=0;off<corrupt.size();off+=1024){size_t n=std::min<size_t>(1024,corrupt.size()-off);assert(MusicPackage::append(t,off,hex(corrupt,off,n)));}
  assert(!MusicPackage::finish(t));assert(MusicPackage::ready());fakeMillis+=1000;
  // An interrupted installation retains the previously committed package after reboot.
  assert(MusicPackage::start(data.size(),t));assert(MusicPackage::append(t,0,hex(data,0,1024)));
  fakeMillis+=31000;MusicPackage::expire();fakeMillis+=1000;assert(!MusicPackage::busy());MusicPackage::begin();assert(MusicPackage::ready());
  upload(data);MusicPackage::begin();assert(MusicPackage::ready());
  std::cout<<"Music storage transaction tests passed\n";
}
