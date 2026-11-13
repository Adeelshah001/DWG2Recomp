#include <cstdint>
#pragma once
#include "camera_control.h"
namespace dwg2::camera {
struct ProjectionSample {float original_angle=0,aspect=0,x=0,y=0;uint32_t mode=0;};
inline bool MatchesProjection(const ProjectionSample& s,int boost) {
 if(s.mode||!ValidPercent(boost)||!std::isfinite(s.original_angle)||!std::isfinite(s.aspect)||s.aspect<=0||!std::isfinite(s.x)||!std::isfinite(s.y))return false;
 const double angle=AdjustedAngle(s.original_angle,boost);
 if(!(angle>0&&angle<3.141592653589793))return false;
 const double y=1/std::tan(angle/2),x=y/s.aspect;
 return std::abs(s.x-x)<=0.0001*(1+std::abs(x))&&std::abs(s.y-y)<=0.0001*(1+std::abs(y));
}
inline const char* ProjectionKind(const ProjectionSample& s,int boost) {
 if(MatchesProjection(s,boost))return "selected";
 if(MatchesProjection(s,0))return "native";
 return "other"; // Nonstandard/orthographic matrices are observational only.
}
}
