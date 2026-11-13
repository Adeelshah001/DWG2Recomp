extern "C" void __imp__sub_82167A80(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82167728(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8212AE38(PPCContext&,uint8_t*);
static thread_local Dwg2PauseGraphics dwg2_main_graphics;
static bool Dwg2MainGraphicsModel(PPCContext& ctx,uint8_t* base,uint32_t owner) {
 auto& state=dwg2_main_graphics;if(!dwg2_menu_memory||Dwg2GraphicsQuery(0)<1)return false;
 if(!state.model){state.model=dwg2_menu_memory->SystemHeapAlloc(8192,16);if(!state.model)return false;std::memset(base+state.model,0,8192);base[state.model+3]=79;base[state.model+1736]=5;}
 state.owner=owner;REX_STORE_U32(state.model+1732,state.active?1:0);return Dwg2GraphicsRowsSync(state.model,base,true);
}
extern "C" void sub_82167A80(PPCContext& ctx,uint8_t* base) {
 const auto owner=ctx.r3.u32;dwg2_main_graphics.active=false;__imp__sub_82167A80(ctx,base);
 if(!dwg2::menus::InsertGraphics(base+owner+24,5,5))throw std::runtime_error("Unexpected main Options layout");
 Dwg2MenuCall(__imp__sub_821292D8,ctx,base,owner+24,REX_LOAD_U32(owner+8),uint32_t(-1));
 REXLOG_INFO("DWG2 main Options: native Graphics/Video row added, object=0x{:08X}",owner);
}
extern "C" void sub_82168370(PPCContext& ctx,uint8_t* base) {
 const auto owner=ctx.r3.u32;auto& state=dwg2_main_graphics;
 const uint32_t pressed=REX_LOAD_U32(0x826C39F8),repeat=REX_LOAD_U32(0x826C39FC),confirm=REX_LOAD_U32(0x82697094),cancel=confirm==8192?4096:8192;
 if(state.active&&state.owner==owner) {
  Dwg2MenuCall(__imp__sub_8212AE38,ctx,base,owner+24);
  Dwg2MenuCall(__imp__sub_821B4B08,ctx,base,state.model);
  if(pressed&cancel)state.active=false;
  else {Dwg2MenuCall(__imp__sub_82145078,ctx,base,state.model+1737,4,0);if(!(repeat&3)&&(pressed&12))Dwg2GraphicsAdjust(state.model,base,(pressed&8)?1:-1);}
  Dwg2MainGraphicsModel(ctx,base,owner);ctx.r3.u64=1;return;
 }
 const bool input_ready=std::bit_cast<float>(REX_LOAD_U32(owner+20))<=0&&REX_LOAD_U32(owner+1820)==5&&!(REX_LOAD_U32(0x826DAE90)&0x80000000u);
 if(input_ready&&REX_LOAD_U32(owner+8)==5&&!REX_LOAD_U32(owner+12)&&!REX_LOAD_U32(owner+16)&&!REX_LOAD_U32(owner+1748)&&(pressed&confirm)&&!(pressed&3)&&!(repeat&3)) {
  if(Dwg2MainGraphicsModel(ctx,base,owner)){state.active=true;REX_STORE_U32(state.model+1732,1);}ctx.r3.u64=1;return;
 }
 Dwg2ExpandedMainOptionsUpdate(ctx,base);
}
extern "C" void sub_82167728(PPCContext& ctx,uint8_t* base) {
 const auto owner=ctx.r3.u32;const auto previous_label=dwg2_graphics_label_selected;dwg2_graphics_label_selected=REX_LOAD_U32(owner+8)==5;__imp__sub_82167728(ctx,base);dwg2_graphics_label_selected=previous_label;
 if(REX_LOAD_U32(owner+8)==5&&!REX_LOAD_U32(owner+16)&&!REX_LOAD_U32(owner+1748)&&Dwg2MainGraphicsModel(ctx,base,owner))Dwg2MenuCall(sub_821B4BC8,ctx,base,dwg2_main_graphics.model,dwg2_main_graphics.model);
}
