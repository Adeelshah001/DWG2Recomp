// Reuse the original font and every original textbox/background draw.
#include "pc_storage_text.h"
extern "C" void __imp__sub_82133FC0(PPCContext&,uint8_t*);
extern "C" void sub_82133FC0(PPCContext& ctx,uint8_t* base) {
 const uint32_t pointer=ctx.r4.u32,stack=ctx.r1.u32;
 if(pointer<4096||uint64_t(pointer)+2048>0x100000000ULL||stack<8192){__imp__sub_82133FC0(ctx,base);return;}
 std::string input;input.reserve(128);bool terminated=false;
 for(unsigned i=0;i<2048;++i){char c=char(base[pointer+i]);if(!c){terminated=true;break;}input+=c;}
 if(!terminated){__imp__sub_82133FC0(ctx,base);return;}
 auto text=dwg2::storage::DisplayText(input);
 if(text==input||text.size()>767){__imp__sub_82133FC0(ctx,base);return;}
 const uint64_t original_stack=ctx.r1.u64;ctx.r1.u32-=2048;const uint32_t scratch=ctx.r1.u32+256;
 std::memcpy(base+scratch,text.c_str(),text.size()+1);ctx.r4.u32=scratch;
 struct Restore{PPCContext&ctx;uint64_t stack;~Restore(){ctx.r1.u64=stack;}}restore{ctx,original_stack};
 __imp__sub_82133FC0(ctx,base);
}
