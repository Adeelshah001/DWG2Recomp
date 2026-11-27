#pragma once
#include "fov_visibility.h"
namespace dwg2::camera {
enum class FovCallKind { None, TrigHalfAngle, TanHalfAngle, FullAngle };
constexpr FovCallKind ClassifyFovCaller(uint32_t caller) {
 switch(caller) {
  case 0x8240F378u: case 0x8240FBA8u: case 0x82410828u: case 0x824123F8u: case 0x8241216Cu:
  case 0x824109D0u: case 0x82410B2Cu: case 0x82412360u: case 0x82412630u: case 0x82125E70u:
   return FovCallKind::TrigHalfAngle;
  case 0x823ED064u: case 0x823A4A18u: case 0x823C90CCu:
   return FovCallKind::TanHalfAngle;
  case 0x823A47F4u:
   return FovCallKind::FullAngle;
  default: return FovCallKind::None;
 }
}
inline bool ScaleFovInput(const ProjectionSample& s,int boost,uint32_t caller,FovCallKind expected,float input,float& output) {
 if(expected==FovCallKind::None||ClassifyFovCaller(caller)!=expected||!boost||!MatchesProjection(s,boost))return false;
 const float scale=expected==FovCallKind::FullAngle?1.0f:0.5f;
 if(!std::isfinite(input)||std::abs(input-s.original_angle*scale)>0.000001f)return false;
 output=float(AdjustedAngle(s.original_angle,boost)*double(scale));
 return output!=input;
}
inline bool EffectsHalfAngle(const ProjectionSample& s,int boost,uint32_t caller,float input,float& output) {
 return ScaleFovInput(s,boost,caller,FovCallKind::TrigHalfAngle,input,output);
}
}
