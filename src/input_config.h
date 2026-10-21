// Original host input mapping. No guest code or assets.
#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <sstream>
#include <string>

namespace dwg2::input {
enum Action { A, B, X, Y, LB, RB, LT, RT, Back, Start, L3, R3,
              Up, Down, Left, Right, LUp, LDown, LLeft, LRight,
              RUp, RDown, RLeft, RRight, Fullscreen, Count };
inline constexpr const wchar_t* names[Count]={L"A / Cross",L"B / Circle",L"X / Square",L"Y / Triangle",
 L"LB / L1",L"RB / R1",L"LT / L2",L"RT / R2",L"Back / Create",L"Start / Options",
 L"Left stick click",L"Right stick click",L"D-pad Up",L"D-pad Down",L"D-pad Left",L"D-pad Right",
 L"Left stick Up",L"Left stick Down",L"Left stick Left",L"Left stick Right",
 L"Right stick Up",L"Right stick Down",L"Right stick Left",L"Right stick Right",L"Fullscreen"};
inline constexpr unsigned short masks[16]={0x1000,0x2000,0x4000,0x8000,0x100,0x200,0,0,
 0x20,0x10,0x40,0x80,1,2,4,8};
// kind: 0 unbound, 1 keyboard virtual key, 2 SDL gamepad button, 3 SDL axis half.
struct Binding { int kind=0, code=0, sign=1, mods=0; };
struct Profile { std::array<Binding,Count> bind{}, alternate{}; int deadzone=0; };
struct Raw { std::array<bool,256> keys{}; int mods=0; std::array<bool,26> buttons{};
             std::array<float,6> axes{}; };
struct Output { unsigned short buttons=0; unsigned char lt=0,rt=0; short lx=0,ly=0,rx=0,ry=0; bool fullscreen=false; };
inline std::mutex mutex;
inline std::atomic<bool> editing{false}, pause_pressed{false};
inline std::atomic<unsigned> generation{1};
inline Profile Defaults(int type) {
 Profile p;
 if(type==0) {
  const int keys[Count]={32,8,'L','P','1','3','Q','E','Z',13,'F','K',38,40,37,39,
                        'W','S','A','D',38,40,37,39,13};
  for(int i=0;i<Count;++i)p.bind[i]={1,keys[i],1,(i>=Up && i<=Right)?1:0};
  p.bind[Fullscreen].mods=4;
  p.alternate[A]={1,186};p.alternate[B]={1,222};p.alternate[LT]={1,'I'};
  p.alternate[RT]={1,'O'};p.alternate[Back]={1,9};p.alternate[Start]={1,'X'};
 } else {
  // SDL standardizes Xbox and DualSense positions before our per-family remap.
  const int buttons[16]={0,1,2,3,9,10,-1,-1,4,6,7,8,11,12,13,14};
  for(int i=0;i<16;++i) if(buttons[i]>=0)p.bind[i]={2,buttons[i]};
  p.bind[LT]={3,4,1};p.bind[RT]={3,5,1};
  for(int i=LUp;i<=RRight;++i) {
   int local=(i-LUp)%4, stick=(i-LUp)/4;
   p.bind[i]={3,stick*2+(local<2?1:0),(local==0 || local==2)?-1:1};
  }
  if(type==1)p.bind[Fullscreen]={2,20}; // DualSense touchpad click.
 }
 return p;
}
inline std::array<Profile,3> profiles{Defaults(0),Defaults(1),Defaults(2)};
inline Profile Get(int type) { std::lock_guard lock(mutex);return profiles.at(type); }
inline void Set(int type,const Profile& p) { std::lock_guard lock(mutex);profiles.at(type)=p;generation.fetch_add(1); }
inline bool Valid(Binding b,int type) {
 if(b.kind==0)return true;
 if(b.mods<0 || b.mods>7 || (b.sign!=1 && b.sign!=-1))return false;
 if(type==0)return b.kind==1 && b.code>0 && b.code<256;
 return (b.kind==2 && b.code>=0 && b.code<26 && b.mods==0) ||
        (b.kind==3 && b.code>=0 && b.code<6 && b.mods==0);
}
inline float Level(Binding b,const Raw& raw,int deadzone) {
 if(!b.kind)return 0;
 if(b.kind==1) {
  int mods=raw.mods;
  if(b.code==16)mods&=~1; if(b.code==17)mods&=~2; if(b.code==18)mods&=~4;
  return mods==b.mods && raw.keys[b.code]?1.f:0.f;
 }
 if(b.kind==2)return raw.buttons[b.code]?1.f:0.f;
 const float value=std::max(0.f,raw.axes[b.code]*b.sign), dz=deadzone/100.f;
 return value<=dz?0.f:std::clamp((value-dz)/(1.f-dz),0.f,1.f);
}
inline Output Map(const Profile& p,const Raw& raw) {
 Output out;std::array<float,Count> levels{};
 for(int i=0;i<Count;++i)levels[i]=std::max(Level(p.bind[i],raw,p.deadzone),Level(p.alternate[i],raw,p.deadzone));
 for(int i=0;i<16;++i)if(levels[i]>.5f)out.buttons|=masks[i];
 out.lt=static_cast<unsigned char>(levels[LT]*255+.5f);out.rt=static_cast<unsigned char>(levels[RT]*255+.5f);
 auto axis=[&](int positive,int negative){return static_cast<short>(std::clamp(levels[positive]-levels[negative],-1.f,1.f)*32767.f);};
 out.lx=axis(LRight,LLeft);out.ly=axis(LUp,LDown);out.rx=axis(RRight,RLeft);out.ry=axis(RUp,RDown);
 out.fullscreen=levels[Fullscreen]>.5f;return out;
}
inline std::string Serialize(const std::array<Profile,3>& data) {
 std::ostringstream out;out<<"DWG2_INPUT 1\n";
 for(const auto& p:data) { out<<p.deadzone<<'\n';
  for(int i=0;i<Count;++i)for(auto b:{p.bind[i],p.alternate[i]})out<<b.kind<<' '<<b.code<<' '<<b.sign<<' '<<b.mods<<'\n';
 }
 return out.str();
}
inline bool Parse(const std::string& text,std::array<Profile,3>& result) {
 std::istringstream in(text);std::string magic;int version=0;
 if(!(in>>magic>>version) || magic!="DWG2_INPUT" || version!=1)return false;
 auto data=result;
 for(int type=0;type<3;++type) {
  auto& p=data[type];if(!(in>>p.deadzone) || p.deadzone<0 || p.deadzone>50)return false;
  for(int i=0;i<Count;++i)for(auto* b:{&p.bind[i],&p.alternate[i]})
   if(!(in>>b->kind>>b->code>>b->sign>>b->mods) || !Valid(*b,type))return false;
 }
 in>>std::ws;if(!in.eof())return false;result=data;return true;
}
template<class Gamepad> inline void Write(const Output& o,Gamepad& p) {
 p.buttons=o.buttons;p.left_trigger=o.lt;p.right_trigger=o.rt;
 p.thumb_lx=o.lx;p.thumb_ly=o.ly;p.thumb_rx=o.rx;p.thumb_ry=o.ry;
}
}
