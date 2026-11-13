// Original host-side extension of the title's bounded native menu record list.
// No baked game artwork is embedded. Labels are rendered through the native font.
#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
namespace dwg2::menus {
constexpr uint32_t kRecordBytes=140,kCapacity=12,kCountOffset=1680;
constexpr uint32_t kGraphicsWhite=0xFFFFF001u,kGraphicsGold=0xFFFFF002u,kGraphicsLine=0xFFFFF003u;
inline uint32_t Load(const uint8_t* p) {
 return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
}
inline void Store(uint8_t* p,uint32_t n) {p[0]=uint8_t(n>>24);p[1]=uint8_t(n>>16);p[2]=uint8_t(n>>8);p[3]=uint8_t(n);}
inline bool CustomTexture(uint32_t value) {return value>=kGraphicsWhite&&value<=kGraphicsLine;}
// Call only after native initialization and before its selection/transition reset.
// Layout is the actual 12-record native list, not a resized guest allocation.
inline bool InsertGraphics(uint8_t* list,uint32_t original_count,uint32_t at) {
 if((original_count!=5&&original_count!=7)||at!=5)throw std::invalid_argument("Unsupported native menu layout");
 const auto count=Load(list+kCountOffset);
 if(count==original_count+1) {
  const auto* row=list+at*kRecordBytes;
  return Load(row+84)==kGraphicsWhite&&Load(row+100)==kGraphicsGold&&Load(row+116)==kGraphicsLine;
 }
 if(count!=original_count||original_count>=kCapacity)return false;
 uint8_t row[kRecordBytes];std::memcpy(row,list+(at-1)*kRecordBytes,kRecordBytes);
 // Route the three normal sprite passes through the native-font interception.
 // Zero source coordinates select the ordinary rectangular sprite entry, whose
 // custom handles must be intercepted before reaching a texture dereference.
 std::memset(row+84,0,48);
 // Keep all three real texture handles intact: native transition code also
 // reads texture metadata before reaching the draw function. Tag source U
 // coordinates instead, consumed before the native rectangular draw executes.
 Store(row+84,kGraphicsWhite);Store(row+100,kGraphicsGold);Store(row+116,kGraphicsLine);
 std::memmove(list+(at+1)*kRecordBytes,list+at*kRecordBytes,(original_count-at)*kRecordBytes);
 std::memcpy(list+at*kRecordBytes,row,kRecordBytes);Store(list+kCountOffset,original_count+1);return true;
}
enum class Action { NativeChild,Graphics,Quit,Resume,Invalid };
struct Selection {Action action;uint32_t native_row;};
inline Selection BriefingSelection(uint32_t row) {
 if(row<5)return {Action::NativeChild,row};
 if(row==5)return {Action::Graphics,0};
 if(row==6)return {Action::Quit,5};
 if(row==7)return {Action::Resume,6};
 return {Action::Invalid,0};
}
inline bool ReflowBriefing(uint8_t* list) {
 if(Load(list+kCountOffset)!=8)return false;
 for(uint32_t row=0;row<8;++row)for(uint32_t field:{20u,24u,44u,48u,68u,72u}){
  auto* p=list+row*kRecordBytes+field;
  if(Load(p)==161+row*64)Store(p,161+row*56);
 }
 return true;
}
}
