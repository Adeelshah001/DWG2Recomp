#include <cfenv>
#include <array>
#include <windows.h>
static void Dwg2MenuTrace(const char* stage,uint32_t object,unsigned kind,uint32_t controller,uint8_t* base){
 if(!object||controller>1||uint64_t(object)+(kind?8:1744)>0x100000000ULL)return;
 struct Entry{uint32_t object=0;const char*stage=nullptr;unsigned controller=0;uint64_t tick=0;};static thread_local std::array<Entry,16>entries{};static thread_local unsigned next=0,budget=0;static thread_local uint64_t window=0;const auto now=GetTickCount64();if(now-window>=5000){window=now;budget=24;}if(!budget)return;
 Entry*e=nullptr;for(auto&x:entries)if(x.object==object&&x.stage==stage&&x.controller==controller){e=&x;break;}if(e&&now-e->tick<5000)return;if(!e)e=&entries[next++%entries.size()];*e={object,stage,controller,now};--budget;
 std::fenv_t saved;std::fegetenv(&saved);
 if(!kind)REXLOG_INFO("DWG2 native menu: stage={} object=0x{:08X} kind=options pane={} left={} right={} right_count={}",stage,object,REX_LOAD_U32(object+1732),unsigned(base[object+1736+DWG2_PHYS_OFFSET(object+1736)]),unsigned(base[object+1737+DWG2_PHYS_OFFSET(object+1737)]),unsigned(base[object+1739+DWG2_PHYS_OFFSET(object+1739)]));
 else REXLOG_INFO("DWG2 native menu: stage={} object=0x{:08X} kind=briefing controller={} selected={} active_child=0x{:08X}",stage,object,controller,unsigned(base[object+controller+DWG2_PHYS_OFFSET(object+controller)]),REX_LOAD_U32(object+4));
 std::fesetenv(&saved);
}
extern "C" void __imp__sub_821B4760(PPCContext&,uint8_t*);
extern "C" void sub_821B4760(PPCContext&ctx,uint8_t*base){uint32_t object=ctx.r3.u32;__imp__sub_821B4760(ctx,base);Dwg2MenuTrace("options_initialized",object,0,0,base);}
extern "C" void __imp__sub_821B4828(PPCContext&,uint8_t*);
extern "C" void sub_821B4828(PPCContext&ctx,uint8_t*base){Dwg2MenuTrace("options_update",ctx.r3.u32,0,0,base);__imp__sub_821B4828(ctx,base);}
extern "C" void __imp__sub_821B4E80(PPCContext&,uint8_t*);
extern "C" void sub_821B4E80(PPCContext&ctx,uint8_t*base){Dwg2MenuTrace("options_draw",ctx.r4.u32,0,0,base);Dwg2SettingsOptionsDraw(ctx,base);}
extern "C" void __imp__sub_8227E7B0(PPCContext&,uint8_t*);
extern "C" void sub_8227E7B0(PPCContext&ctx,uint8_t*base){Dwg2MenuTrace("briefing_update",ctx.r3.u32,1,ctx.r4.u32,base);Dwg2SettingsBriefingUpdate(ctx,base);}
extern "C" void __imp__sub_8227E410(PPCContext&,uint8_t*);
extern "C" void sub_8227E410(PPCContext&ctx,uint8_t*base){Dwg2MenuTrace("briefing_draw",ctx.r3.u32,1,0,base);Dwg2SettingsBriefingDraw(ctx,base);}
