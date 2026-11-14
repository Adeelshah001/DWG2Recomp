#include <rex/ppc/context.h>
#include <rex/image_info.h>
#include "camera_control.h"
#include <bit>
extern "C" __declspec(dllimport) int Dwg2FovPercent();
extern "C" void __imp__sub_82386940(PPCContext&,uint8_t*);
// Original host hook over the generated weak alias. Direct and indirect calls
// both reach this entry; the always-original generated implementation is kept.
extern "C" void sub_82386940(PPCContext& ctx,uint8_t* base) {
  ctx.f1.f64=dwg2::camera::AdjustedAngle(ctx.f1.f64,Dwg2FovPercent());
  __imp__sub_82386940(ctx,base);
}
// Guest implementations and mappings are generated from the user's ISO locally.
extern const rex::PPCImageInfo PPCImageConfig;
extern "C" void sub_821669F8(PPCContext&, uint8_t*);
extern "C" void sub_82166B38(PPCContext&, uint8_t*);
// Same volatile big-endian accesses and physical-window offset as the pinned SDK.
#ifndef REX_LOAD_U32
#if REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64)
#define DWG2_PHYS_OFFSET(x) ((uint32_t(x) >= 0xE0000000u) ? 0x1000u : 0u)
#else
#define DWG2_PHYS_OFFSET(x) 0u
#endif
#define REX_LOAD_U32(x) __builtin_bswap32(*(volatile uint32_t*)(base + uint32_t(x) + DWG2_PHYS_OFFSET(x)))
#define REX_STORE_U32(x,y) (*(volatile uint32_t*)(base + uint32_t(x) + DWG2_PHYS_OFFSET(x)) = __builtin_bswap32(y))
#endif
#include <rex/rex_app.h>
#include <rex/system/function_dispatcher.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include "camera_refresh.h"
extern "C" void __imp__sub_8211A220(PPCContext&,uint8_t*);
extern "C" void __imp__sub_823862B8(PPCContext&,uint8_t*);
static thread_local dwg2::camera::RefreshTracker dwg2_camera_refresh;
static void Dwg2RefreshCamera(PPCContext& ctx,uint8_t* base) {
  const uint32_t object=ctx.r3.u32;
  const int percent=Dwg2FovPercent();
  if(dwg2_camera_refresh.NeedsRefresh(object,percent)) {
    const uint32_t dirty=object+353;
    *(volatile uint8_t*)(base+dirty+DWG2_PHYS_OFFSET(dirty))=1;
    if(percent)
      REXLOG_INFO("DWG2 FOV: refreshing camera 0x{:08X}, projection mode {}, original angle {}, boost {}%",
        object,REX_LOAD_U32(object+344),std::bit_cast<float>(REX_LOAD_U32(object+328)),percent);
  }
}
extern "C" void sub_8211A220(PPCContext& ctx,uint8_t* base) {
  Dwg2RefreshCamera(ctx,base);__imp__sub_8211A220(ctx,base);
}
extern "C" void sub_823862B8(PPCContext& ctx,uint8_t* base) {
  Dwg2RefreshCamera(ctx,base);__imp__sub_823862B8(ctx,base);
}

#include "fov_visibility.h"
#include <cfenv>
#include <array>
#include <windows.h>
// Observational only: no visibility return, matrix, range, scene pointer, or
// guest register is changed. Logs remain private and are bounded per thread.
static void Dwg2VisibilityTrace(const char* stage,unsigned phase,uint32_t data,uint32_t output,uint8_t* base) {
 if(!data||uint64_t(data)+336>0x100000000ULL)return;
 const int boost=Dwg2FovPercent();
 struct Entry {uint32_t data=0;unsigned phase=0;int boost=-1;ULONGLONG tick=0;};
 static thread_local std::array<Entry,32> entries{};
 static thread_local unsigned next=0,budget=0;
 static thread_local ULONGLONG window=0;
 const auto now=GetTickCount64();if(now-window>=5000){window=now;budget=24;}
 if(!budget)return;
 Entry* selected=nullptr;for(auto& e:entries)if(e.data==data&&e.phase==phase){selected=&e;break;}
 if(selected&&selected->boost==boost&&now-selected->tick<5000)return;
 if(!selected)selected=&entries[next++%entries.size()];
 *selected={data,phase,boost,now};--budget;
 struct FloatingEnvironment {std::fenv_t saved;FloatingEnvironment(){std::fegetenv(&saved);}~FloatingEnvironment(){std::fesetenv(&saved);}} preserve;
 auto read=[&](uint32_t offset){return std::bit_cast<float>(REX_LOAD_U32(data+offset));};
 dwg2::camera::ProjectionSample sample{read(312),read(316),read(176),read(196),REX_LOAD_U32(data+328)};
 REXLOG_INFO("DWG2 FOV visibility: stage={} camera_data=0x{:08X} boost={} mode={} original_angle={} aspect={} projection_x={} projection_y={} projection_kind={}",
   stage,data,boost,sample.mode,sample.original_angle,sample.aspect,sample.x,sample.y,dwg2::camera::ProjectionKind(sample,boost));
 if(phase==1&&output&&uint64_t(output)+176<=0x100000000ULL)
  REXLOG_INFO("DWG2 FOV bounds: object=0x{:08X} left={} bottom={} right={} top={}",output,
   std::bit_cast<float>(REX_LOAD_U32(output+144)),std::bit_cast<float>(REX_LOAD_U32(output+148)),
   std::bit_cast<float>(REX_LOAD_U32(output+160)),std::bit_cast<float>(REX_LOAD_U32(output+164)));
}
extern "C" void __imp__sub_82225750(PPCContext&,uint8_t*);
extern "C" void sub_82225750(PPCContext& ctx,uint8_t* base) {
 Dwg2VisibilityTrace("scene_camera_input",0,ctx.r5.u32,0,base);__imp__sub_82225750(ctx,base);
}
extern "C" void __imp__sub_8222ADB8(PPCContext&,uint8_t*);
extern "C" void sub_8222ADB8(PPCContext& ctx,uint8_t* base) {
 const uint32_t data=ctx.r4.u32,object=ctx.r3.u32;
 __imp__sub_8222ADB8(ctx,base);Dwg2VisibilityTrace("scene_bounds",1,data,object,base);
}
extern "C" void __imp__sub_82223710(PPCContext&,uint8_t*);
extern "C" void sub_82223710(PPCContext& ctx,uint8_t* base) {
 Dwg2VisibilityTrace("visibility_matrix_input",2,ctx.r5.u32,0,base);__imp__sub_82223710(ctx,base);
}

#include "fov_effects.h"
extern "C" void __imp__sub_821244B8(PPCContext&,uint8_t*);
extern "C" void sub_821244B8(PPCContext& ctx,uint8_t* base) {
 // This exact call constructs the effects frustum from camera payload+312.
 // Other vector trigonometry callers retain their original input.
 if(uint32_t(ctx.lr)==0x8240F378) {
  const uint32_t data=ctx.r30.u32;
  if(data&&uint64_t(data)+336<=0x100000000ULL) {
   const int boost=Dwg2FovPercent();float adjusted=0;bool apply=false;
   {std::fenv_t saved;std::fegetenv(&saved);
    auto read=[&](uint32_t o){return std::bit_cast<float>(REX_LOAD_U32(data+o));};
    dwg2::camera::ProjectionSample sample{read(312),read(316),read(176),read(196),REX_LOAD_U32(data+328)};
    apply=dwg2::camera::EffectsHalfAngle(sample,boost,uint32_t(ctx.lr),ctx.v1.f32[3],adjusted);
    std::fesetenv(&saved);
   }
   if(apply){Dwg2VisibilityTrace("effects_frustum_corrected",3,data,0,base);ctx.v1.f32[3]=adjusted;}
  }
 }
 __imp__sub_821244B8(ctx,base);
}

#ifndef REX_LOAD_U8
#define REX_LOAD_U8(x) (*(volatile uint8_t*)(base+uint32_t(x)+DWG2_PHYS_OFFSET(x)))
#define REX_STORE_U8(x,y) (*(volatile uint8_t*)(base+uint32_t(x)+DWG2_PHYS_OFFSET(x))=uint8_t(y))
#endif
extern "C" void sub_8216AB80(PPCContext&,uint8_t*);
#include "native_menu_expanded.inc"
#include "native_settings_ui.inc"
#include "native_settings_widgets.inc"
#include "pc_storage_text.inc"
#include "startup_notice.inc"
#include "title_quit.inc"
#include "native_main_graphics.inc"



#include "native_menu_trace.inc"
#include "startup_load.h"
#include "startup_notice_policy.h"
#include "automatic_save.h"
#include "save_write_guard.inc"
extern "C" void __imp__sub_82115E08(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82114688(PPCContext&,uint8_t*);
static std::atomic<bool> dwg2_startup_load_attempted{false};
static std::atomic<bool> dwg2_startup_storage_window{true};
static bool Dwg2StartupStorageWindow(){return dwg2_startup_storage_window.load()&&!dwg2_startup_load_attempted.load();}
extern "C" void sub_82115E08(PPCContext& ctx,uint8_t* base) {
  const uint32_t object=ctx.r3.u32,caller=uint32_t(ctx.lr);
  auto begin_load=[&] {
    if(caller!=0x82176B00||dwg2_startup_load_attempted.load())return;
    dwg2::startup::LoadSnapshot s;
    s.caller=caller;s.mode=REX_LOAD_U32(object+1752);s.state=REX_LOAD_U32(object+1748);
    if(s.mode!=2||s.state!=5)return;
    s.buffer=REX_LOAD_U32(object+1760);
    if(!s.buffer||uint64_t(s.buffer)+0x00334000>0x100000000ULL)return;
    s.magic=REX_LOAD_U32(s.buffer);s.count=REX_LOAD_U32(s.buffer+4);
    s.stride=REX_LOAD_U32(s.buffer+8);s.occupied=REX_LOAD_U32(s.buffer+12);
    s.slot_type=REX_LOAD_U32(s.buffer+16);s.data_offset=REX_LOAD_U32(s.buffer+36);
    s.interim_offset=REX_LOAD_U32(s.buffer+40);
    if(!dwg2::startup::CanLoadFirstSlot(s))return;
    bool expected=false;if(!dwg2_startup_load_attempted.compare_exchange_strong(expected,true))return;
    // Enter the same load operation as confirming occupied slot 01 in startup
    // mode. Keep the original ready checks, data application and error paths.
    REX_STORE_U32(object+1756,0);REX_STORE_U32(object+1748,7);
    PPCContext action=ctx;action.r3.u32=object;action.r4.u32=4;
    __imp__sub_82114688(action,base);
    // Only the cosmetic startup hold is shortened. No I/O completion/result is
    // manufactured, and manual load/save dialogs retain their original timing.
    REX_STORE_U32(object+1772,25);REX_STORE_U32(object+1824,0);
    REXLOG_INFO("DWG2 startup: loading slot 01 through original load operation");
  };
  auto begin_save=[&] {
    dwg2::saving::Snapshot s;s.caller=caller;
    if(caller!=0x8216D360&&caller!=0x8227E76C)return;
    s.mode=REX_LOAD_U32(object+1752);s.state=REX_LOAD_U32(object+1748);s.ready=REX_LOAD_U32(object+1808);
    if(s.mode!=0||s.state!=5||s.ready!=2)return;
    s.buffer=REX_LOAD_U32(object+1760);
    if(!s.buffer||uint64_t(s.buffer)+0x00334000>0x100000000ULL)return;
    s.magic=REX_LOAD_U32(s.buffer);s.count=REX_LOAD_U32(s.buffer+4);s.stride=REX_LOAD_U32(s.buffer+8);
    s.occupied=REX_LOAD_U32(s.buffer+12);s.data_offset=REX_LOAD_U32(s.buffer+36);
    if(!dwg2::saving::CanSaveFirstSlot(s))return;
    // Same state/message as accepting an overwrite of slot01. The original
    // state7 save operation, completion polling and error presentation run.
    // No write, serializer result, success flag or I/O wait is fabricated.
    REX_STORE_U32(object+1756,0);REX_STORE_U32(object+1748,7);
    PPCContext action=ctx;action.r3.u32=object;action.r4.u32=10;
    __imp__sub_82114688(action,base);
    REXLOG_INFO("DWG2 automatic save: selected slot01 through original save operation, caller={:08X}",caller);
  };
  begin_load();begin_save();__imp__sub_82115E08(ctx,base);begin_load();begin_save();
  if(caller==0x82176B00&&ctx.r3.u32==0)dwg2_startup_storage_window.store(false);
}
REXCVAR_DEFINE_BOOL(dwg2_skip_startup_logos, true, "DWG2", "Skip startup branding using original pre-title storage transition");
static void Dwg2BootInit(PPCContext& ctx, uint8_t* base) {
  const uint32_t object = ctx.r3.u32;
  sub_821669F8(ctx, base);
  if (REXCVAR_GET(dwg2_skip_startup_logos) &&
      REX_LOAD_U32(object) == 0x82036DE4 && REX_LOAD_U32(object+12) == 1) {
    // Original initialization/resources are complete. State 7 retains the original
    // storage readiness checks, title transition, and subsequent resource cleanup.
    REX_STORE_U32(object+8, 31);
    REX_STORE_U32(object+12, 7);
    REXLOG_INFO("DWG2 startup: branding skipped; original storage/title transition retained");
  }
}
static void Dwg2BootUpdate(PPCContext& ctx, uint8_t* base) {
  const uint32_t object = ctx.r3.u32;
  sub_82166B38(ctx, base);
  if (REXCVAR_GET(dwg2_skip_startup_logos) &&
      REX_LOAD_U32(object) == 0x82036DE4 &&
      REX_LOAD_U32(object+12) == 7 &&
      REX_LOAD_U32(0x826C39A4) == 2 &&
      REX_LOAD_U32(0x8269FCA0) == 83 &&
      REX_LOAD_U32(0x8269FCA4) == 3) {
    // Match the original opening movie completion branch: loading state 18,
    // substate 0. Storage readiness and boot resource cleanup remain original.
    REX_STORE_U32(0x82698D54, 0);
    REX_STORE_U32(0x826C39A4, 18);
    REXLOG_INFO("DWG2 startup: opening movie bypassed through original completion transition");
  }
}
class Dwg2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Dwg2App>(new Dwg2App(ctx, "dwg2", PPCImageConfig));
  }
 protected:
  void OnPostLaunchModule(rex::system::XThread*) override {
    dwg2_menu_memory=runtime()->memory();
    dwg2_title_graphics=static_cast<rex::graphics::GraphicsSystem*>(runtime()->graphics_system());
    dwg2_title_window.store(window()?static_cast<HWND>(window()->GetNativeWindowHandle()):nullptr);
    auto* dispatcher = runtime()->function_dispatcher();
    if(!dispatcher->GetFunction(0x823EDD90u))throw std::runtime_error("Local generator omitted required guest routine");
    if(dispatcher->GetFunction(0x82133FC0)!=&sub_82133FC0)throw std::runtime_error("PC storage text hook is not active");
    if(dispatcher->GetFunction(0x82177278)!=&sub_82177278||dispatcher->GetFunction(0x821771D8)!=&sub_821771D8||dispatcher->GetFunction(0x821772E8)!=&sub_821772E8||dispatcher->GetFunction(0x82176938)!=&sub_82176938)
      throw std::runtime_error("Title quit hooks do not match the supplied game build");
    if(dispatcher->GetFunction(0x82386940)!=&sub_82386940)
      throw std::runtime_error("DWG2 perspective hook does not match this game build");
    if(dispatcher->GetFunction(0x8211A220)!=&sub_8211A220||
       dispatcher->GetFunction(0x823862B8)!=&sub_823862B8)
      throw std::runtime_error("DWG2 camera refresh hooks do not match this game build");
    if(dispatcher->GetFunction(0x821244B8)!=&sub_821244B8)
      throw std::runtime_error("DWG2 effects frustum hook does not match this build");
    if(dispatcher->GetFunction(0x82225750)!=&sub_82225750||dispatcher->GetFunction(0x8222ADB8)!=&sub_8222ADB8||dispatcher->GetFunction(0x82223710)!=&sub_82223710)
      throw std::runtime_error("DWG2 FOV visibility diagnostics do not match this build");
    if(dispatcher->GetFunction(0x821B4760)!=&sub_821B4760||dispatcher->GetFunction(0x821B4828)!=&sub_821B4828||dispatcher->GetFunction(0x821B4E80)!=&sub_821B4E80||dispatcher->GetFunction(0x8227E7B0)!=&sub_8227E7B0||dispatcher->GetFunction(0x8227E410)!=&sub_8227E410)
      throw std::runtime_error("DWG2 native-menu diagnostic hooks do not match this build");
    if(dispatcher->GetFunction(0x821B4880)!=&sub_821B4880||dispatcher->GetFunction(0x8227DB78)!=&sub_8227DB78||dispatcher->GetFunction(0x821B4380)!=&sub_821B4380||dispatcher->GetFunction(0x82130238)!=&sub_82130238)
      throw std::runtime_error("DWG2 native Graphics/Video hooks do not match this candidate");
    if(dispatcher->GetFunction(0x82167A80)!=&sub_82167A80||dispatcher->GetFunction(0x82168370)!=&sub_82168370||dispatcher->GetFunction(0x82167728)!=&sub_82167728||dispatcher->GetFunction(0x821B4BC8)!=&sub_821B4BC8||dispatcher->GetFunction(0x82130188)!=&sub_82130188)
      throw std::runtime_error("DWG2 main Options/settings widget hooks do not match this candidate");
    if(dispatcher->GetFunction(0x8216AB80)!=&sub_8216AB80)throw std::runtime_error("DWG2 Controls nine-way navigation repair is not active");
    if(dispatcher->GetFunction(0x82115E08)!=&sub_82115E08)
      throw std::runtime_error("DWG2 startup load hook does not match this game build");
    if (dispatcher->GetFunction(0x821669F8) != &sub_821669F8 ||
        !dispatcher->SetFunction(0x821669F8, &Dwg2BootInit) ||
        dispatcher->GetFunction(0x82166B38) != &sub_82166B38 ||
        !dispatcher->SetFunction(0x82166B38, &Dwg2BootUpdate))
      throw std::runtime_error("DWG2 startup hook does not match this game build");
  }
};
REX_DEFINE_APP(dwg2, Dwg2App::Create)
