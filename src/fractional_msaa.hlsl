// Original host-only MSAA capture into 3/2 retained tiles.
cbuffer Parameters : register(b0) {
 uint GuestWidth,GuestHeight,GuestPitch,SourceX;
 uint SourceY,SampleSelect,CaptureFlags,Reserved;
};
Texture2DMS<float4> Source : register(t0);
RWByteAddressBuffer Retained : register(u0);
[numthreads(8,8,1)]
void CaptureMSAA(uint3 thread:SV_DispatchThreadID,uint3 group:SV_GroupID) {
 uint tilesX=GuestPitch/32;uint tile=group.z;
 uint2 local=thread.xy;uint2 pixel=uint2(tile%tilesX,tile/tilesX)*48+local;
 uint2 extent=(uint2(GuestWidth,GuestHeight)*3+1)/2;
 uint packed=0;
 if(all(pixel<extent)) {
  uint2 sampleAt=pixel;if(CaptureFlags&1)sampleAt=max(sampleAt,uint2(1,1));sampleAt+=uint2(SourceX,SourceY);
  float4 color;
  if(Reserved) {
   // Guest2x sample0 is top, sample1 bottom; map to host2x or4x.
   uint top=Reserved==1?1:0,bottom=Reserved==1?0:3;
   if(SampleSelect==0)color=Source.Load(sampleAt,top);
   else if(SampleSelect==1)color=Source.Load(sampleAt,bottom);
   else color=(Source.Load(sampleAt,top)+Source.Load(sampleAt,bottom))*.5f;
  }
  else if(SampleSelect<4)color=Source.Load(sampleAt,SampleSelect);
  else if(SampleSelect<6){uint first=SampleSelect==4?0:2;color=(Source.Load(sampleAt,first)+Source.Load(sampleAt,first+1))*.5f;}
  else color=(Source.Load(sampleAt,0)+Source.Load(sampleAt,1)+Source.Load(sampleAt,2)+Source.Load(sampleAt,3))*.25f;
  uint4 bytes=uint4(round(saturate(color)*255.f));if(CaptureFlags&2)bytes.rb=bytes.br;
  packed=bytes.r|(bytes.g<<8)|(bytes.b<<16)|(bytes.a<<24);
 }
 Retained.Store((tile*48*48+local.y*48+local.x)*4,packed);
}
