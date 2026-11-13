// Original texture identification and glyph rendering; contains no game pixels.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>
#include "ui_atlas_fingerprints.h"
#include "input_art_decode.h"

namespace dwg2::atlas {
struct Fingerprint { uint64_t a=14695981039346656037ULL,b=7809847782465536322ULL; };
inline Fingerprint Hash(std::span<const uint8_t> pixels) {
  Fingerprint f;
  for(uint8_t c:pixels){f.a=(f.a^c)*1099511628211ULL;f.b=(f.b^c)*14029467366897019727ULL;}
  return f;
}
inline int Identify(std::span<const uint8_t> rgba,int width,int height) {
  if(rgba.size()!=size_t(width)*height*4)return 0;
  bool possible=false;for(const auto& f:fingerprints)possible|=f.width==width&&f.height==height;
  if(!possible)return 0;
  const auto h=Hash(rgba);
  for(const auto& f:fingerprints)if(f.width==width&&f.height==height&&f.a==h.a&&f.b==h.b)return f.kind;
  return 0;
}
struct Rect { int x,y,w,h; };
inline void Icon(std::vector<uint8_t>& p,int width,Rect r,unsigned glyph) {
  const auto& mask=dwg2::art::Glyph(glyph);
  for(int y=0;y<r.h;++y)for(int x=0;x<r.w;++x) {
    // Resample the vector-derived mask, retaining antialiased icon edges.
    float sx=8.f+(x+.5f)*48.f/r.w-.5f,sy=8.f+(y+.5f)*48.f/r.h-.5f;
    int ix=int(std::floor(sx)),iy=int(std::floor(sy));float fx=sx-ix,fy=sy-iy;
    auto alpha=[&](int px,int py){return float(mask[size_t(std::clamp(py,0,63))*64+std::clamp(px,0,63)]);};
    float coverage=(alpha(ix,iy)*(1-fx)+alpha(ix+1,iy)*fx)*(1-fy)+(alpha(ix,iy+1)*(1-fx)+alpha(ix+1,iy+1)*fx)*fy;
    size_t at=(size_t(r.y+y)*width+r.x+x)*4;
    float dx=(x+.5f-r.w*.5f)/(r.w*.5f),dy=(y+.5f-r.h*.5f)/(r.h*.5f);
    float background=glyph<4&&dx*dx+dy*dy<.99f?230.f:0.f;
    p[at]=p[at+1]=p[at+2]=uint8_t(22+coverage*223.f/255.f);
    p[at+3]=uint8_t(std::max(coverage,background));
  }
}
inline void Face(std::vector<uint8_t>& p,int width,Rect r,int shape) {Icon(p,width,r,unsigned(shape));}
inline void Label(std::vector<uint8_t>& p,int width,Rect r,const char* text) {
 unsigned glyph=10;
 if(text[0]=='L')glyph=text[1]=='1'?6:text[1]=='2'?8:4;
 else if(text[0]=='R')glyph=text[1]=='1'?7:text[1]=='2'?9:5;
 else if(text[0]=='S')glyph=11;
 Icon(p,width,r,glyph);
}
inline bool PlayStation(std::vector<uint8_t>& rgba,int width,int height,int kind) {
  if(rgba.size()!=size_t(width)*height*4)return false;
  if(kind==2&&width==512&&height==256) {
    Face(rgba,width,{72,72,72,72},0); // A -> Cross
    Face(rgba,width,{0,72,72,72},1);  // B -> Circle
    Face(rgba,width,{396,0,72,72},2); // X -> Square
    Face(rgba,width,{324,0,72,72},3); // Y -> Triangle
    return true;
  }
  if(kind==3&&width==1024&&height==512) {
    Face(rgba,width,{298,250,24,24},0);Face(rgba,width,{326,250,24,24},1);
    Face(rgba,width,{707,166,24,24},2);Face(rgba,width,{395,178,24,24},3);
    Label(rgba,width,{422,208,25,20},"L1");Label(rgba,width,{918,98,26,23},"R1");
    Label(rgba,width,{397,206,20,24},"L2");Label(rgba,width,{846,143,21,25},"R2");
    // RS/LS guest cells: RS is right stick, LS is left stick.
    Label(rgba,width,{298,222,24,24},"R3");Label(rgba,width,{675,201,24,24},"L3");
    Label(rgba,width,{439,175,36,17},"OPT");Label(rgba,width,{791,139,32,17},"SHR");
    return true;
  }
  if(kind==4&&width==1024&&height==512) {
    Label(rgba,width,{144,207,64,29},"R1");
    Label(rgba,width,{687,170,46,28},"L1");
    return true;
  }
  return false;
}
}
