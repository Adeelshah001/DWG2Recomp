#pragma once
#include <atomic>
#include <cmath>
namespace dwg2::camera {
inline std::atomic<bool> extended{false};
// Adjust the perspective angle, not the viewport or HUD. The game supplies a
// fresh angle on each call, so live toggles cannot accumulate camera drift.
inline double ExtendedAngle(double radians) {
 constexpr double pi=3.14159265358979323846;
 return std::isfinite(radians)&&radians>0&&radians<pi/1.1?radians*1.1:radians;
}
}
