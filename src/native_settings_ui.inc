#include "native_menu_layout.h"
#include "native_menu_font.inc"
#include <rex/memory.h>
#include <unordered_map>
#include <vector>
#include <cstdio>
static rex::memory::Memory* dwg2_menu_memory=nullptr;
#include "menu_label_data.h"
extern "C" void __imp__sub_822E3990(PPCContext&,uint8_t*);
static uint32_t Dwg2BakedLabelResource(PPCContext&ctx,uint8_t*base,bool white) {
 struct Label {uint32_t data=0,resource=0;bool attempted=false;};static thread_local Label labels[2];auto& label=labels[white?1:0];
 if(label.attempted)return label.resource;if(!dwg2_menu_memory)return 0;label.attempted=true;
 const uint8_t* bytes=white?dwg2::baked::white:dwg2::baked::gold;const size_t size=white?sizeof(dwg2::baked::white):sizeof(dwg2::baked::gold);
 label.data=dwg2_menu_memory->SystemHeapAlloc(size,16);if(!label.data)return 0;std::memcpy(base+label.data,bytes,size);
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);ctx.r3.u32=label.data;ctx.r4.u32=uint32_t(size);ctx.r5.u32=0;
 __imp__sub_822E3990(ctx,base);label.resource=ctx.r3.u32;ctx=saved;std::fesetenv(&environment);
 REXLOG_INFO("DWG2 baked menu label: style={} resource=0x{:08X}",white?"white":"gold",label.resource);return label.resource;
}

static thread_local uint32_t dwg2_pause_list=0;
extern "C" void __imp__sub_821B4880(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8227DB78(PPCContext&,uint8_t*);
extern "C" void __imp__sub_821292D8(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82130238(PPCContext&,uint8_t*);
extern "C" void __imp__sub_821B4760(PPCContext&,uint8_t*);
extern "C" void __imp__sub_821B4B08(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82145078(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8212AB48(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8212AC70(PPCContext&,uint8_t*);
static void Dwg2ReflowPause(uint32_t list,uint8_t* base) {
 if(list&&list==dwg2_pause_list)dwg2::menus::ReflowBriefing(base+list);
}
extern "C" void sub_821292D8(PPCContext& ctx,uint8_t* base) {const auto list=ctx.r3.u32;__imp__sub_821292D8(ctx,base);Dwg2ReflowPause(list,base);}
extern "C" void sub_8212AB48(PPCContext& ctx,uint8_t* base) {const auto list=ctx.r3.u32;__imp__sub_8212AB48(ctx,base);Dwg2ReflowPause(list,base);}
extern "C" void sub_8212AC70(PPCContext& ctx,uint8_t* base) {const auto list=ctx.r3.u32;__imp__sub_8212AC70(ctx,base);Dwg2ReflowPause(list,base);}
using Dwg2NativeFn=void(*)(PPCContext&,uint8_t*);
static void Dwg2MenuCall(Dwg2NativeFn fn,PPCContext& ctx,uint8_t* base,uint32_t a,uint32_t b=0,uint32_t c=0) {
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
 ctx.r3.u32=a;ctx.r4.u32=b;ctx.r5.u32=c;fn(ctx,base);ctx=saved;std::fesetenv(&environment);
}
static int Dwg2GraphicsQuery(int key) {
 using Fn=int(*)(int);static auto fn=(Fn)GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"Dwg2GraphicsValue");return fn?fn(key):-1;
}
static bool Dwg2GraphicsChange(int key,int value) {
 using Fn=int(*)(int,int);static auto fn=(Fn)GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"Dwg2GraphicsRequest");return fn&&fn(key,value)!=0;
}
struct Dwg2GraphicsRows {uint8_t count=0;std::vector<uint8_t> previous;};
static thread_local std::unordered_map<uint32_t,Dwg2GraphicsRows> dwg2_graphics_rows;
struct Dwg2PauseGraphics {uint32_t owner=0,model=0,controller=0;bool active=false;};
static thread_local Dwg2PauseGraphics dwg2_pause_graphics;
static bool Dwg2GraphicsRowsSync(uint32_t model,uint8_t* base,bool active) {
 auto found=dwg2_graphics_rows.find(model);
 if(!active) {
  if(found!=dwg2_graphics_rows.end()){
   auto& old=found->second;std::memcpy(base+model+1748,old.previous.data(),old.previous.size());base[model+1739]=old.count;dwg2_graphics_rows.erase(found);
  }return true;
 }
 if(found==dwg2_graphics_rows.end()) {
  const uint8_t count=base[model+1739];if(count>32)return false;
  Dwg2GraphicsRows old;old.count=count;old.previous.assign(base+model+1748,base+model+1748+count*132);dwg2_graphics_rows.emplace(model,std::move(old));
 }
 const int scale=Dwg2GraphicsQuery(0),fov=Dwg2GraphicsQuery(1),fullscreen=Dwg2GraphicsQuery(2);
 const int vsync=Dwg2GraphicsQuery(4);
 char rows[4][128]{};
 if(scale<1||fov<0||fullscreen<0)return false;
 std::snprintf(rows[0],128,"Resolution");
 std::snprintf(rows[1],128,"Field of View");
 std::snprintf(rows[2],128,"Fullscreen");
 std::snprintf(rows[3],128,"VSync");
 for(unsigned i=0;i<4;++i){auto* row=base+model+1748+i*132;std::memset(row,0,132);std::memcpy(row,rows[i],std::strlen(rows[i])+1);}
 base[model+1739]=4;if(base[model+1737]>=4)base[model+1737]=0;base[model+1738]=0;return true;
}
static void Dwg2GraphicsAdjust(uint32_t model,uint8_t* base,int delta) {
 const uint32_t row=base[model+1737];if(row>=4)return;
 const int key=row==3?4:int(row),current=Dwg2GraphicsQuery(key);if(current<0)return;
 int next=current;
 if(row==0){if(Dwg2GraphicsQuery(3)>0)return;next=std::max(1,std::min(3,current+delta));}
 else if(row==1)next=std::max(0,std::min(50,current+delta*5));
 else next=current?0:1;
 if(next!=current)Dwg2GraphicsChange(key,next);
}
extern "C" void sub_821B4880(PPCContext& ctx,uint8_t* base) {
 const uint32_t object=ctx.r3.u32,model=ctx.r4.u32;dwg2_graphics_rows.erase(model);
 __imp__sub_821B4880(ctx,base);
 if(!dwg2::menus::InsertGraphics(base+object+8,5,5))throw std::runtime_error("Unexpected Options native row layout");
 Dwg2MenuCall(__imp__sub_821292D8,ctx,base,object+8,base[model+1736],uint32_t(-1));
}
extern "C" void sub_8227DB78(PPCContext& ctx,uint8_t* base) {
 const uint32_t object=ctx.r3.u32;dwg2_pause_graphics.active=false;
 __imp__sub_8227DB78(ctx,base);
 if(!dwg2::menus::InsertGraphics(base+object+9472,7,5))throw std::runtime_error("Unexpected briefing native row layout");
 dwg2_pause_list=object+9472;
 Dwg2MenuCall(__imp__sub_821292D8,ctx,base,object+9472,base[object],uint32_t(-1));
 Dwg2ReflowPause(object+9472,base);
}
extern "C" void sub_82130238(PPCContext& ctx,uint8_t* base) {
 if(!Dwg2DrawCustomMenuSprite(ctx,base))__imp__sub_82130238(ctx,base);
}
extern "C" void sub_821B4380(PPCContext& ctx,uint8_t* base) {
 const uint32_t model=ctx.r3.u32;const bool graphics=base[model+1736]==5;
 if(!Dwg2GraphicsRowsSync(model,base,graphics)){ctx.r3.u64=0;return;}
 const uint32_t pressed=REX_LOAD_U32(0x826C39F8),repeat=REX_LOAD_U32(0x826C39FC),confirm=REX_LOAD_U32(0x82697094);
 if(graphics&&!(repeat&3)) {
  if(REX_LOAD_U32(model+1732)==0&&(pressed&confirm)){REX_STORE_U32(model+1732,1);ctx.r3.u64=0;return;}
  if(REX_LOAD_U32(model+1732)==1&&(pressed&12)){Dwg2GraphicsAdjust(model,base,(pressed&8)?1:-1);ctx.r3.u64=0;return;}
 }
 Dwg2ExpandedOptionsUpdate(ctx,base);Dwg2GraphicsRowsSync(model,base,base[model+1736]==5);
}
static bool Dwg2OpenPauseGraphics(PPCContext& ctx,uint8_t* base,uint32_t owner,uint32_t controller) {
 if(!dwg2_menu_memory||Dwg2GraphicsQuery(0)<1)return false;
 auto& state=dwg2_pause_graphics;
 if(!state.model)state.model=dwg2_menu_memory->SystemHeapAlloc(8192,16);
 if(!state.model)return false;
 // Build only the UI: full Options initialization resets the live player profile.
 std::memset(base+state.model,0,8192);Dwg2MenuCall(sub_821B4880,ctx,base,state.model,state.model);
 auto* list=base+state.model+8;std::memcpy(list,list+5*140,140);dwg2::menus::Store(list+1680,1);
 base[state.model+1736]=5;REX_STORE_U32(state.model+1732,1);
 Dwg2MenuCall(__imp__sub_821292D8,ctx,base,state.model+8,0,uint32_t(-1));
 if(!Dwg2GraphicsRowsSync(state.model,base,true))return false;
 state.owner=owner;state.controller=controller;state.active=true;return true;
}
static void Dwg2SettingsBriefingUpdate(PPCContext& ctx,uint8_t* base) {
 const uint32_t owner=ctx.r3.u32,controller=ctx.r4.u32;
 auto& state=dwg2_pause_graphics;
 if(controller>1){Dwg2ExpandedBriefingUpdate(ctx,base);return;}
 const uint32_t input=0x826C3998+68*controller,pressed=REX_LOAD_U32(input+96),repeat=REX_LOAD_U32(input+100),confirm=REX_LOAD_U32(0x82697094),cancel=confirm==8192?4096:8192;
 if(state.active&&state.owner==owner) {
  if(controller==state.controller) {
   if(pressed&cancel)state.active=false;
   else {
    Dwg2MenuCall(__imp__sub_821B4B08,ctx,base,state.model);
    Dwg2MenuCall(__imp__sub_82145078,ctx,base,state.model+1737,4,controller);
    if(!(repeat&3)&&(pressed&12))Dwg2GraphicsAdjust(state.model,base,(pressed&8)?1:-1);
    Dwg2GraphicsRowsSync(state.model,base,true);
   }
  }ctx.r3.u64=0;return;
 }
 if(!REX_LOAD_U32(owner+4)&&base[owner+controller]==5&&!(repeat&3)&&(pressed&confirm)) {
  Dwg2OpenPauseGraphics(ctx,base,owner,controller);ctx.r3.u64=0;return;
 }
 Dwg2ExpandedBriefingUpdate(ctx,base);
}
static void Dwg2SettingsBriefingDraw(PPCContext& ctx,uint8_t* base) {
 if(dwg2_pause_graphics.active&&dwg2_pause_graphics.owner==ctx.r3.u32) {
  const auto saved=ctx;ctx.r3.u32=dwg2_pause_graphics.model;ctx.r4.u32=ctx.r3.u32;Dwg2ExpandedOptionsDraw(ctx,base);ctx=saved;
 }else Dwg2ExpandedBriefingDraw(ctx,base);
}
