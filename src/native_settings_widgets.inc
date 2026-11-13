extern "C" void __imp__sub_821B4BC8(PPCContext&,uint8_t*);
#include <string>
extern "C" void __imp__sub_82130188(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8211AA28(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8212DC40(PPCContext&,uint8_t*);
static thread_local uint32_t dwg2_graphics_render_model=0;
static thread_local bool dwg2_graphics_hint_scope=false;
extern "C" void __imp__sub_82130E30(PPCContext&,uint8_t*);
extern "C" void sub_82130E30(PPCContext& ctx,uint8_t* base) {
 if(dwg2_graphics_hint_scope&&ctx.r1.u32>=4096&&ctx.r3.u32&&uint64_t(ctx.r3.u32)+1024<=0x100000000ULL){
  std::string text;for(unsigned i=0;i<1024;++i){char c=char(REX_LOAD_U8(ctx.r3.u32+i));if(!c)break;text+=c;}
  const auto at=text.find("Updates");if(text.size()<1024&&at!=std::string::npos){
   text.replace(at,7,"Settings");const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);ctx.r1.u32-=2048;ctx.r3.u32=ctx.r1.u32+160;
   for(unsigned i=0;i<=text.size();++i)REX_STORE_U8(ctx.r3.u32+i,i<text.size()?uint8_t(text[i]):0);
   __imp__sub_82130E30(ctx,base);ctx=saved;std::fesetenv(&environment);return;
  }
 }__imp__sub_82130E30(ctx,base);
}
static void Dwg2SettingsOptionsDraw(PPCContext& ctx,uint8_t* base){const auto previous=dwg2_graphics_hint_scope;dwg2_graphics_hint_scope=base[ctx.r4.u32+1736]==5;const auto previous_label=dwg2_graphics_label_selected;dwg2_graphics_label_selected=dwg2_graphics_hint_scope;Dwg2ExpandedOptionsDraw(ctx,base);dwg2_graphics_label_selected=previous_label;dwg2_graphics_hint_scope=previous;}
static void Dwg2WidgetText(PPCContext& ctx,uint8_t* base,const char* text,int x,int y,unsigned height=28,uint32_t color=0xFFFFFFFFu) {
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
 constexpr uint32_t font=0x826DAEB0u,color_address=font+0x1A844u,width_address=font+0x1A81Au,height_address=font+0x1A81Cu;
 const uint8_t width_hi=base[width_address],width_lo=base[width_address+1];
 const auto old_color=REX_LOAD_U32(color_address);const uint8_t hi=base[height_address],lo=base[height_address+1];
 const auto length=std::strlen(text);if(length>127||ctx.r1.u32<2048)return;
 ctx.r1.u32-=1024;std::memcpy(base+ctx.r1.u32+160,text,length+1);
 REX_STORE_U32(color_address,color);base[width_address]=uint8_t(height>>8);base[width_address+1]=uint8_t(height);base[height_address]=uint8_t(height>>8);base[height_address+1]=uint8_t(height);
 ctx.r3.u32=font;ctx.r4.u32=ctx.r1.u32+160;ctx.f1.f64=x;ctx.f2.f64=y;ctx.r7.u32=1;ctx.r8.u32=1;ctx.r9.u32=1080;ctx.r10.s32=-1;
 __imp__sub_821340C0(ctx,base);
 REX_STORE_U32(color_address,old_color);base[height_address]=hi;base[height_address+1]=lo;base[width_address]=width_hi;base[width_address+1]=width_lo;ctx=saved;std::fesetenv(&environment);
}
static void Dwg2WidgetRect(PPCContext& ctx,uint8_t* base,int x,int y,int width,int height,uint32_t rgba) {
 const auto saved=ctx;std::fenv_t environment;std::fegetenv(&environment);
 if(ctx.r1.u32<2048)return;ctx.r1.u32-=512;const auto rect=ctx.r1.u32+128,col=rect+16;
 REX_STORE_U32(rect,uint32_t(x));REX_STORE_U32(rect+4,uint32_t(y));REX_STORE_U32(rect+8,uint32_t(width));REX_STORE_U32(rect+12,uint32_t(height));REX_STORE_U32(col,rgba);
 ctx.r3.u32=1;__imp__sub_8212DC40(ctx,base);ctx.r1.u32=saved.r1.u32-512;ctx.r3.u32=rect;ctx.r4.u32=col;__imp__sub_8211AA28(ctx,base);
 ctx=saved;std::fesetenv(&environment);
}
static void Dwg2WidgetBar(PPCContext& ctx,uint8_t* base,int y,unsigned selected,unsigned intervals,bool active) {
 constexpr int left=710,width=330;
 const uint32_t gold=active?0xE4CAA6FFu:0xB19D82C0u;
 Dwg2WidgetRect(ctx,base,left,y,width,4,0x4A3939FFu);
 const int position=left+int(selected)*width/int(intervals);
 if(position>left)Dwg2WidgetRect(ctx,base,left,y,position-left,4,gold);
 for(unsigned i=0;i<=intervals;++i)Dwg2WidgetRect(ctx,base,left+int(i)*width/int(intervals)-1,y-3,2,10,gold);
 Dwg2WidgetRect(ctx,base,position-5,y-6,10,16,0xF6EAD3FFu);
}
static void Dwg2GraphicsWidgets(PPCContext& ctx,uint8_t* base,uint32_t model) {
 const bool active=REX_LOAD_U32(model+1732)==1;const uint32_t color=active?0xFFFFFFFFu:0xFFFFFFA0u;
 Dwg2WidgetText(ctx,base,Dwg2GraphicsQuery(2)>0?"ON":"OFF",704,388,28,color);
 Dwg2WidgetText(ctx,base,Dwg2GraphicsQuery(4)>0?"ON":"OFF",704,476,28,color);
 const int scale=Dwg2GraphicsQuery(0),fov=Dwg2GraphicsQuery(1);if(scale<1||scale>3||fov<0||fov>50)return;
 char text[80];std::snprintf(text,sizeof(text),"%dx (%d x %dp)",scale,1280*scale,720*scale);
 Dwg2WidgetText(ctx,base,text,704,212,28,color);Dwg2WidgetBar(ctx,base,252,unsigned(scale-1),2,active&&base[model+1737]==0);
 Dwg2WidgetText(ctx,base,"1x",704,265,16,color);Dwg2WidgetText(ctx,base,"2x",869,265,16,color);Dwg2WidgetText(ctx,base,"3x",1030,265,16,color);
 if(fov)std::snprintf(text,sizeof(text),"+%d%%",fov);else std::snprintf(text,sizeof(text),"Default");
 Dwg2WidgetText(ctx,base,text,704,300,28,color);Dwg2WidgetBar(ctx,base,340,unsigned(fov/5),10,active&&base[model+1737]==1);
 Dwg2WidgetText(ctx,base,"Default",704,353,16,color);Dwg2WidgetText(ctx,base,"+50%",1000,353,16,color);
}
extern "C" void sub_82130188(PPCContext& ctx,uint8_t* base) {
 if(dwg2_graphics_render_model&&ctx.r4.s32==488&&ctx.r5.s32==162){const uint32_t alpha=REX_LOAD_U32(dwg2_graphics_render_model+1732)==1?255:160;Dwg2WidgetText(ctx,base,"Settings",488,162,32,0xFFFFFF00u|alpha);return;}
 __imp__sub_82130188(ctx,base);
}
extern "C" void sub_821B4BC8(PPCContext& ctx,uint8_t* base) {
 const uint32_t model=ctx.r4.u32,previous=dwg2_graphics_render_model;
 if(base[model+1736]==5)dwg2_graphics_render_model=model;
 __imp__sub_821B4BC8(ctx,base);
 if(dwg2_graphics_render_model==model)Dwg2GraphicsWidgets(ctx,base,model);
 dwg2_graphics_render_model=previous;
}

#include "graphics_cell_geometry.inc"
