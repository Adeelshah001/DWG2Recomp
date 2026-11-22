// Original host compute shader: retain individual D24S8 MSAA depth/stencil samples.
cbuffer Parameters : register(b0) {
 uint GuestWidth,GuestHeight,GuestPitch,SourceX;
 uint SourceY,SampleSelect,CaptureFlags,Guest2Mapping;
};
Texture2DMS<float> Depth : register(t0);
Texture2DMS<uint2> Stencil : register(t1);
RWByteAddressBuffer Retained : register(u0);
[numthreads(8,8,1)]
void CaptureDepthMSAA(uint3 thread:SV_DispatchThreadID,uint3 group:SV_GroupID) {
 uint tilesX=GuestPitch/32,tile=group.z;
 uint2 local=thread.xy,pixel=uint2(tile%tilesX,tile/tilesX)*48+local;
 uint2 extent=(uint2(GuestWidth,GuestHeight)*3+1)/2;
 uint packed=0;
 if(all(pixel<extent)) {
  uint2 source=pixel;
  if(CaptureFlags&1)source=max(source,uint2(1,1));
  source+=uint2(SourceX,SourceY);
  uint sample=SampleSelect;
  if(Guest2Mapping)sample=SampleSelect==0?(Guest2Mapping==1?1:0):(Guest2Mapping==1?0:3);
  uint depth=uint(round(saturate(Depth.Load(source,sample))*16777215.f));
  packed=(depth<<8)|(Stencil.Load(source,sample).y&255);
 }
 Retained.Store((tile*48*48+local.y*48+local.x)*4,packed);
}
