#pragma once
#include "fov_visibility.h"
namespace dwg2::camera {
inline bool EffectsHalfAngle(const ProjectionSample& s,int boost,uint32_t caller,float input,float& output) {
 if(caller!=0x8240F378||!boost||!MatchesProjection(s,boost))return false;
 const float half=s.original_angle*0.5f;
 if(!std::isfinite(input)||std::abs(input-half)>0.000001f)return false;
 output=float(AdjustedAngle(s.original_angle,boost)*0.5);
 return output!=input;
}
}
