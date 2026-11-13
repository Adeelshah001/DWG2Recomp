// Extend only Graphics/Video row bottoms; native row origins, text, sliders,
// and88px row spacing stay unchanged. Six extra pixels leave a2px gap.
extern "C" void __imp__sub_82114BC8(PPCContext&,uint8_t*);
extern "C" void __imp__sub_8212B538(PPCContext&,uint8_t*);
static void Dwg2GraphicsCellBottom(PPCContext&ctx,uint8_t*base,void(*draw)(PPCContext&,uint8_t*),uint32_t caller,uint32_t height){
 const uint32_t rectangle=ctx.r3.u32;
 if(!dwg2_graphics_render_model||ctx.lr!=caller||rectangle<4096||uint64_t(rectangle)+16>0x100000000ULL||REX_LOAD_U32(rectangle+12)!=height){draw(ctx,base);return;}
 REX_STORE_U32(rectangle+12,height+6);
 struct Restore{uint8_t*base;uint32_t rectangle,height;~Restore(){REX_STORE_U32(rectangle+12,height);}}restore{base,rectangle,height};
 draw(ctx,base);
}
extern "C" void sub_82114BC8(PPCContext&ctx,uint8_t*base){Dwg2GraphicsCellBottom(ctx,base,__imp__sub_82114BC8,0x821B4D98,80);}
extern "C" void sub_8212B538(PPCContext&ctx,uint8_t*base){Dwg2GraphicsCellBottom(ctx,base,__imp__sub_8212B538,0x821B4DF0,96);}
