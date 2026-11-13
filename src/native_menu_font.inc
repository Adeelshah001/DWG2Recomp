// Include after native_menu_layout.h and the generated PPC memory helpers.
// Intercept only the three private handles inserted by InsertGraphics.
#include <cfenv>
extern "C" void __imp__sub_821340C0(PPCContext&,uint8_t*);
static uint32_t Dwg2BakedLabelResource(PPCContext&,uint8_t*,bool);
extern "C" void __imp__sub_82130238(PPCContext&,uint8_t*);
static thread_local bool dwg2_graphics_label_selected=false;
static bool Dwg2DrawCustomMenuSprite(PPCContext& ctx,uint8_t* base) {
 using namespace dwg2::menus;
 const uint32_t handle=ctx.r8.u32;
 if(!CustomTexture(handle))return false;
 if(handle==kGraphicsLine)return true;
 const uint32_t stack=ctx.r1.u32;
 if(stack>=4096)if(const auto resource=Dwg2BakedLabelResource(ctx,base,handle==kGraphicsWhite)) {
  const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
  ctx.r1.u32=stack-512;std::memcpy(base+ctx.r1.u32+80,base+stack+80,32);
  ctx.r3.u32=resource;ctx.r6.u32=480;ctx.r7.u32=60;ctx.r8.u32=0;ctx.r9.u32=0;ctx.r10.u32=512;
  REX_STORE_U32(ctx.r1.u32+84,64);REX_STORE_U32(ctx.r1.u32+108,0);
  __imp__sub_82130238(ctx,base);ctx=saved;std::fesetenv(&environment);return true;
 }

 // The native rectangular-sprite ABI passes RGBA alpha at SP+95 and layer at
 // SP+100. Refuse malformed frames without dereferencing a synthetic handle.
 if(stack<2048||uint64_t(stack)+112>0x100000000ULL)return true;
 const uint8_t alpha=base[stack+95];if(!alpha)return true;
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
 constexpr uint32_t font=0x826DAEB0u,color=font+0x1A844u,width=font+0x1A81Au,height=font+0x1A81Cu;
 const uint32_t previous=REX_LOAD_U32(color);
 const uint8_t width_hi=base[width],width_lo=base[width+1];
 const uint8_t height_hi=base[height],height_lo=base[height+1];
 struct Restore {
  PPCContext& ctx;const PPCContext& saved;uint8_t* base;uint32_t previous;std::fenv_t& environment;uint8_t height_hi,height_lo,width_hi,width_lo;
  ~Restore(){REX_STORE_U32(color,previous);base[height]=height_hi;base[height+1]=height_lo;base[width]=width_hi;base[width+1]=width_lo;ctx=saved;std::fesetenv(&environment);}
 } restore{ctx,saved,base,previous,environment,height_hi,height_lo,width_hi,width_lo};
 ctx.r1.u32=stack-1024;const uint32_t text=ctx.r1.u32+160;
 constexpr char label[]="Graphics / Video";
 std::memcpy(base+text,label,sizeof(label));
 REX_STORE_U32(color,(dwg2_graphics_label_selected?0xFFFFFF00u:0xD8BE9900u)|alpha);
 base[width]=0;base[width+1]=34;base[height]=0;base[height+1]=44;
 ctx.f1.f64=double(saved.r4.s32)+4.0;ctx.f2.f64=double(saved.r5.s32)-3.0;
 ctx.r3.u32=font;ctx.r4.u32=text;ctx.r7.u32=REX_LOAD_U32(stack+100);
 ctx.r8.u32=1;ctx.r9.u32=1052;ctx.r10.s32=-1;
 const auto arguments=ctx;
 __imp__sub_821340C0(ctx,base);
 // The menu's baked labels are bold italic. Preserve its native italic face
 // and use a one-pixel second pass at the same native 44-pixel letter height.
 ctx=arguments;ctx.f1.f64+=1;__imp__sub_821340C0(ctx,base);
 ctx=arguments;ctx.f2.f64+=1;__imp__sub_821340C0(ctx,base);return true;
}
