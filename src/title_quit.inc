#include <rex/graphics/graphics_system.h>
// Original scene3 is the title frontend. Its model+4192==0 is the Press Start
// page; model+0/+4/+1836/+1840 guard the original Start/save transitions.
#include "title_quit_state.h"
static std::atomic<HWND> dwg2_title_window{nullptr};
static thread_local dwg2::title::QuitState dwg2_title_quit;
static thread_local bool dwg2_attract_pending;
static rex::graphics::GraphicsSystem* dwg2_title_graphics;
extern "C" void __imp__sub_821771D8(PPCContext&,uint8_t*);
extern "C" void __imp__sub_821772E8(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82176938(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82379BB8(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82379D30(PPCContext&,uint8_t*);
static bool Dwg2TitleReady(uint32_t owner,uint8_t* base) {
 if(owner<4096||uint64_t(owner)+4356>0x100000000ULL)return false;
 const uint32_t model=owner+8;
 return REX_LOAD_U32(owner)==0x82037480u&&REX_LOAD_U32(0x826C39A0)==3&&
  !(REX_LOAD_U32(0x826DAE90)&0x80000000u)&&REX_LOAD_U32(model+4192)==0&&
  REX_LOAD_U32(model)==0&&int32_t(REX_LOAD_U32(model+4))<=0&&
  REX_LOAD_U32(model+1836)==0&&REX_LOAD_U32(model+1840)==0;
}
static bool Dwg2TitleFocused() {
 const HWND hwnd=dwg2_title_window.load();return hwnd&&GetForegroundWindow()==hwnd;
}
static bool Dwg2TitleRequestClose() {
 const HWND hwnd=dwg2_title_window.load();DWORD pid=0;
 if(!hwnd||!IsWindow(hwnd)||GetForegroundWindow()!=hwnd||!GetWindowThreadProcessId(hwnd,&pid)||pid!=GetCurrentProcessId())return false;
 return PostMessageW(hwnd,WM_CLOSE,0,0)!=0;
}
extern "C" void sub_821771D8(PPCContext& ctx,uint8_t* base) {
 const uint32_t owner=ctx.r3.u32;
 const auto action=dwg2_title_quit.Update(owner,Dwg2TitleReady(owner,base),Dwg2TitleFocused(),
  REX_LOAD_U32(0x826C39F8),REX_LOAD_U32(0x826C39F4),REX_LOAD_U32(0x826C3A3C),REX_LOAD_U32(0x826C3A38));
 if(action==dwg2::title::Action::Forward){__imp__sub_821771D8(ctx,base);return;}
 if(action==dwg2::title::Action::Open)REXLOG_INFO("DWG2 title quit: confirmation opened (controller {})",dwg2_title_quit.controller);
 if(action==dwg2::title::Action::Decline)REXLOG_INFO("DWG2 title quit: declined; Press Start retained");
 if(action==dwg2::title::Action::Accept){
  if(Dwg2TitleRequestClose())REXLOG_INFO("DWG2 title quit: accepted; normal window close requested");
  else {dwg2_title_quit.CloseFailed();REXLOG_WARN("DWG2 title quit: window close not queued; confirmation retained");}
 }
 // Original title update returns1 while this frontend remains active.
 ctx.r3.u64=1;
}
// Original save-confirmation widget and atlas; scratch guest stack only.
extern "C" void __imp__sub_82177278(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8213EF88(PPCContext&,uint8_t*);
extern "C" void __imp__sub_821409B8(PPCContext&,uint8_t*);
static void Dwg2TitleQuitDraw(PPCContext& ctx,uint8_t* base) {
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
 constexpr uint32_t style=0x826DAEB0u+0x1A800u;
 uint8_t fontStyle[96];std::memcpy(fontStyle,base+style,sizeof(fontStyle));
 struct Restore{PPCContext& ctx;const PPCContext& saved;std::fenv_t& fp;uint8_t* base;uint8_t* style;~Restore(){uint8_t queued[4]={base[0x826F56C8u],base[0x826F56C9u],base[0x826F56D2u],base[0x826F56D3u]};std::memcpy(base+0x826F56B0u,style,96);base[0x826F56C8u]=queued[0];base[0x826F56C9u]=queued[1];base[0x826F56D2u]=queued[2];base[0x826F56D3u]=queued[3];ctx=saved;std::fesetenv(&fp);}}restore{ctx,saved,environment,base,fontStyle};
 const uint32_t device=REX_LOAD_U32(0x8295F510);if(!device||ctx.r1.u32<8192)return;
 ctx.r3.u32=device;__imp__sub_82379BB8(ctx,base);if(ctx.r3.s8==0)return;
 ctx=saved;ctx.r1.u32-=4096;
 const uint32_t widget=ctx.r1.u32+256,heading=ctx.r1.u32+1024;
 std::memset(base+widget,0,584);
 ctx.r3.u32=widget;ctx.r4.u32=0;__imp__sub_8213EF88(ctx,base);
 REX_STORE_U32(widget+4,2);REX_STORE_U32(widget+36,255);REX_STORE_U32(widget+48,2);
 auto half=[&](unsigned offset,uint16_t value){base[widget+offset]=uint8_t(value>>8);base[widget+offset+1]=uint8_t(value);};
 half(8,445);half(16,445);half(10,313);half(18,313);half(12,390);half(14,94);half(20,94);
 static thread_local unsigned pulse=0;base[widget+56]=uint8_t(pulse++%60);
 REX_STORE_U32(widget+580,dwg2_title_quit.selectedYes?0:1);
 std::memcpy(base+widget+60,"Quit Game?",11);std::memcpy(base+heading,"Quit Game",10);
 ctx=saved;ctx.r1.u32-=4096;
 Dwg2WidgetRect(ctx,base,0,0,1280,720,0x000000C0u);
 ctx.r3.u32=heading;__imp__sub_82130E30(ctx,base);
 ctx.r3.u32=widget;__imp__sub_821409B8(ctx,base);
 ctx.r3.u32=device;__imp__sub_82379D30(ctx,base);
 static thread_local bool recorded=false;if(!recorded){recorded=true;REXLOG_INFO("DWG2 title quit: original native Yes/No dialog submitted in title draw pass");}
}
extern "C" void sub_82177278(PPCContext& ctx,uint8_t* base) {
 const uint32_t owner=ctx.r3.u32;__imp__sub_82177278(ctx,base);
 if(dwg2_title_quit.visible&&dwg2_title_quit.owner==owner&&Dwg2TitleReady(owner,base))Dwg2TitleQuitDraw(ctx,base);
}
extern "C" void sub_821772E8(PPCContext& ctx,uint8_t* base) {__imp__sub_821772E8(ctx,base);}
extern "C" void sub_82176938(PPCContext& ctx,uint8_t* base) {
 if(dwg2_title_quit.owner==ctx.r3.u32)dwg2_title_quit={};
 __imp__sub_82176938(ctx,base);
}
// The title's attract sequence alternates movie and demo entries.
extern "C" void __imp__sub_82175270(PPCContext&,uint8_t*);
extern "C" void sub_82175270(PPCContext& ctx,uint8_t* base) {
  if (ctx.lr == 0x8217723C) {
    dwg2_attract_pending=true;
    REXLOG_INFO("DWG2 attract: starting {} entry {}",(ctx.r4.u32 & 1)?"movie":"demo",ctx.r4.u32);
  }
  __imp__sub_82175270(ctx,base);
}

// Rebuild GPU backing state before the original title setup starts its fade.
extern "C" void __imp__sub_82175310(PPCContext&,uint8_t*);
extern "C" void sub_82175310(PPCContext& ctx,uint8_t* base) {
  if (std::exchange(dwg2_attract_pending,false) && dwg2_title_graphics) {
    dwg2_title_graphics->InvalidateGpuMemory();
    dwg2_title_graphics->ClearCaches();
    REXLOG_INFO("DWG2 title trace: GPU page invalidation and cache refresh queued at title setup");
  }
  __imp__sub_82175310(ctx,base);
}
