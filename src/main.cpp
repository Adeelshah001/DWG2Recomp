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
#include "startup_load.h"
#include "automatic_save.h"
extern "C" void __imp__sub_82115E08(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82114688(PPCContext&,uint8_t*);
static std::atomic<bool> dwg2_startup_load_attempted{false};
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
    auto* dispatcher = runtime()->function_dispatcher();
    if(dispatcher->GetFunction(0x82386940)!=&sub_82386940)
      throw std::runtime_error("DWG2 perspective hook does not match this game build");
    if(dispatcher->GetFunction(0x8211A220)!=&sub_8211A220||
       dispatcher->GetFunction(0x823862B8)!=&sub_823862B8)
      throw std::runtime_error("DWG2 camera refresh hooks do not match this game build");
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
