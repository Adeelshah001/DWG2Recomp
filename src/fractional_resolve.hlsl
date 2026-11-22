// Original experimental 3/2 resolve storage kernels. Not enabled in the player
// renderer yet. Each 32x32 guest tile retains a complete 48x48 host sample grid;
// CPU readback is a separate operation and never replaces the retained samples.
cbuffer Layout : register(b0) {
  uint guest_width,guest_height,source_width,source_height;
  uint pixel_bytes,tiles_x,guest_pitch,padding;
};
ByteAddressBuffer source : register(t0);
RWByteAddressBuffer destination : register(u0);

// The inverse of retained-tile packing. This writes a row-pitched linear copy
// buffer for D3D12TextureCache, without selecting a single native-resolution
// sample or losing the extra 3/2-resolution detail. 'padding' holds row pitch.
[numthreads(8,8,1)]
void LoadRetainedLinear(uint3 thread : SV_DispatchThreadID) {
  uint lanes=pixel_bytes<4?4/pixel_bytes:1;
  uint x=thread.x*lanes,y=thread.y;
  if(x>=source_width||y>=source_height)return;
  uint output_value=0;
  for(uint lane=0;lane<lanes;++lane) {
    uint px=x+lane;if(px>=source_width)break;
    uint tile=(y/48u)*tiles_x+px/48u;
    uint offset=(tile*48u*48u+(y%48u)*48u+px%48u)*pixel_bytes;
    uint value=source.Load(offset&~3u);
    if(pixel_bytes==8){destination.Store2(y*padding+px*8u,source.Load2(offset));return;}
    if(pixel_bytes==4){destination.Store(y*padding+px*4u,value);return;}
    uint mask=pixel_bytes==2?65535u:255u;
    output_value|=((value>>((offset&3u)*8u))&mask)<<(lane*pixel_bytes*8u);
  }
  destination.Store(y*padding+x*pixel_bytes,output_value);
}

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
uint2 GuestByteOrder(uint2 value) {
  // This entry uses padding as the Xenos 32-bit byte-order mode. Other shader
  // entries use that root constant as their linear row pitch instead.
  uint order=padding&3u;
  if(order==1u)return ((value<<8u)&0xFF00FF00u)|((value>>8u)&0x00FF00FFu);
  if(order==2u)return ((value&255u)<<24u)|((value&65280u)<<8u)|((value>>8u)&65280u)|(value>>24u);
  if(order==3u)return (value<<16u)|(value>>16u);
  return value;
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
    // Texture copies have a 256-byte aligned row pitch. Zero keeps the tight
    // layout used by buffer producers; neither path reads the row padding.
    if(at.x<source_width&&at.y<source_height) {
      uint pitch=padding&0x3FFFFFFFu;
      uint2 sample_at=(padding&0x80000000u)?max(at,uint2(1,1)):at;
      value=ReadPixel(sample_at.y*(pitch?pitch:source_width*pixel_bytes)+sample_at.x*pixel_bytes);
      if((padding&0x40000000u)&&pixel_bytes==4)
        value.x=(value.x&0xFF00FF00u)|((value.x&255u)<<16u)|((value.x>>16u)&255u);
    }
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
      uint phase=(padding>>2u)&1u;
      uint hx=(6u*(gx%32u)+3u+phase)/4u,hy=(6u*(y%32u)+3u+phase)/4u;
      value=ReadPixel((tile*48u*48u+hy*48u+hx)*pixel_bytes);
    }
    if(pixel_bytes>=4)wide=value;
    else packed|=value.x<<(lane*pixel_bytes*8u);
  }
  uint address=GuestAddress(x,y);
  if(pixel_bytes==8)destination.Store2(address,GuestByteOrder(wide));
  else destination.Store(address,GuestByteOrder(uint2(pixel_bytes==4?wide.x:packed,0)).x);
}

// D24S8 copies expose separate 32-bit depth and 8-bit stencil planes. Preserve
// both in the guest representation: depth in bits8..31, stencil in bits0..7.
[numthreads(8,8,1)]
void PackDepth24(uint3 thread:SV_DispatchThreadID) {
 uint2 at=uint2(thread.z%tiles_x,thread.z/tiles_x)*48+thread.xy;uint value=0;
 if(all(at<uint2(source_width,source_height))) {
  uint2 sampleAt=(padding&0x80000000u)?max(at,uint2(1,1)):at;
  sampleAt+=uint2(guest_width,guest_height); // Source crop origin for this entry.
  uint depth=source.Load(sampleAt.y*(padding&0x3FFFFFFFu)+sampleAt.x*4)&0xFFFFFFu;
  uint stencilAddress=guest_pitch+sampleAt.y*pixel_bytes+sampleAt.x;
  uint stencil=(source.Load(stencilAddress&~3u)>>((stencilAddress&3u)*8u))&255u;
  value=(depth<<8)|stencil;
 }
 destination.Store((thread.z*48*48+thread.y*48+thread.x)*4,value);
}
[numthreads(8,8,1)]
void LoadRetainedDepth24(uint3 thread:SV_DispatchThreadID) {
 if(thread.x>=source_width||thread.y>=source_height)return;
 uint tile=(thread.y/48u)*tiles_x+thread.x/48u;
 uint packed=source.Load((tile*48u*48u+(thread.y%48u)*48u+thread.x%48u)*4);
 destination.Store(thread.y*padding+thread.x*4,asuint(float(packed>>8u)*(1.f/16777215.f)));
}
