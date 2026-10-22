// Original experimental 3/2 resolve storage kernels. Not enabled in the player
// renderer yet. Each 32x32 guest tile retains a complete 48x48 host sample grid;
// CPU readback is a separate operation and never replaces the retained samples.
cbuffer Layout : register(b0) {
  uint guest_width,guest_height,source_width,source_height;
  uint pixel_bytes,tiles_x,guest_pitch,padding;
};
ByteAddressBuffer source : register(t0);
RWByteAddressBuffer destination : register(u0);

uint2 ReadPixel(uint address) {
  if(pixel_bytes==8)return source.Load2(address);
  uint value=source.Load(address&~3u);
  if(pixel_bytes==4)return uint2(value,0);
  uint mask=pixel_bytes==2?65535u:255u;
  return uint2((value>>((address&3u)*8u))&mask,0);
}
// Xbox 360 tiled byte addressing, following the BSD-licensed ReXGlue/Xenia
// texture conversion convention. Native pitches are multiples of 32 texels.
uint GuestAddress(uint x,uint y) {
  uint log_bpp=firstbithigh(pixel_bytes);
  uint row=((y/32u)*(guest_pitch/32u))<<(log_bpp+7u);
  uint micro_y=(y&6u)<<(2u+log_bpp);
  row+=((micro_y&~15u)<<1u)+(micro_y&15u)+((y&8u)<<(3u+log_bpp))+((y&1u)<<4u);
  uint macro=(x/32u)<<(log_bpp+7u),micro=(x&7u)<<log_bpp;
  uint offset=row+macro+((micro&~15u)<<1u)+(micro&15u);
  return ((offset&~511u)<<3u)+((offset&448u)<<2u)+(offset&63u)+((y&16u)<<7u)+
    (((((y&8u)>>2u)+(x>>3u))&3u)<<6u);
}
[numthreads(8,8,1)]
void PackScaled(uint3 thread : SV_DispatchThreadID) {
  uint lanes=pixel_bytes<4?4u/pixel_bytes:1u;
  uint x=thread.x*lanes,y=thread.y,tile=thread.z;
  if(x>=48u||y>=48u)return;
  uint2 origin=uint2(tile%tiles_x,tile/tiles_x)*48u;
  uint packed=0;uint2 wide=0;
  for(uint lane=0;lane<lanes;++lane) {
    uint2 at=origin+uint2(x+lane,y);
    uint2 value=0;
    if(at.x<source_width&&at.y<source_height)value=ReadPixel((at.y*source_width+at.x)*pixel_bytes);
    if(pixel_bytes>=4)wide=value;
    else packed|=value.x<<(lane*pixel_bytes*8u);
  }
  uint address=(tile*48u*48u+y*48u+x)*pixel_bytes;
  if(pixel_bytes==8)destination.Store2(address,wide);
  else destination.Store(address,pixel_bytes==4?wide.x:packed);
}
[numthreads(8,8,1)]
void ReadbackGuest(uint3 thread : SV_DispatchThreadID) {
  uint lanes=pixel_bytes<4?4u/pixel_bytes:1u;
  uint x=thread.x*lanes,y=thread.y;
  if(x>=guest_width||y>=guest_height)return;
  uint packed=0;uint2 wide=0;
  for(uint lane=0;lane<lanes;++lane) {
    uint gx=x+lane;uint2 value=0;
    if(gx<guest_width) {
      uint tile=(y/32u)*tiles_x+gx/32u;
      uint hx=(6u*(gx%32u)+3u)/4u,hy=(6u*(y%32u)+3u)/4u;
      value=ReadPixel((tile*48u*48u+hy*48u+hx)*pixel_bytes);
    }
    if(pixel_bytes>=4)wide=value;
    else packed|=value.x<<(lane*pixel_bytes*8u);
  }
  uint address=GuestAddress(x,y);
  if(pixel_bytes==8)destination.Store2(address,wide);
  else destination.Store(address,pixel_bytes==4?wide.x:packed);
}
