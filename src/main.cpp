#include <rex/ppc/context.h>
#include <rex/image_info.h>
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
    if (dispatcher->GetFunction(0x821669F8) != &sub_821669F8 ||
        !dispatcher->SetFunction(0x821669F8, &Dwg2BootInit) ||
        dispatcher->GetFunction(0x82166B38) != &sub_82166B38 ||
        !dispatcher->SetFunction(0x82166B38, &Dwg2BootUpdate))
      throw std::runtime_error("DWG2 startup hook does not match this game build");
  }
};
REX_DEFINE_APP(dwg2, Dwg2App::Create)
