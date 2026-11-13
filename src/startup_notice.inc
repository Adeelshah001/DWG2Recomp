// This is a blocking Information popup, before the startup load controller.
// Acknowledge only its exact no-device informational text during startup.
extern "C" void __imp__sub_82117290(PPCContext&,uint8_t*);
static bool Dwg2StartupStorageWindow();
extern "C" void sub_82117290(PPCContext&ctx,uint8_t*base){
 const uint32_t pointer=ctx.r3.u32;std::string text;
 if(pointer>=4096&&uint64_t(pointer)+512<=0x100000000ULL)for(unsigned i=0;i<512;++i){char c=char(base[pointer+i]);if(!c)break;text+=c;}
 if(Dwg2StartupStorageWindow()&&text.starts_with("No storage device has been selected.")){
  REXLOG_INFO("DWG2 startup: auto-acknowledged storage Information popup, caller={:08X}",uint32_t(ctx.lr));return;
 }
 __imp__sub_82117290(ctx,base);
}
