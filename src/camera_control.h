#pragma once
#include <atomic>
#include <cmath>
#include <istream>
#include <string>
namespace dwg2::camera {
inline std::atomic<int> percent{0};
constexpr bool ValidPercent(int value) { return value>=0&&value<=50&&value%5==0; }
// Always scale the original perspective angle, never a previously scaled value.
inline double AdjustedAngle(double radians,int value) {
 constexpr double pi=3.14159265358979323846;
 if(!ValidPercent(value)||!value)return radians;
 const double factor=1.0+value/100.0;
 return std::isfinite(radians)&&radians>0&&radians<pi/factor?radians*factor:radians;
}
inline double ExtendedAngle(double radians) { return AdjustedAngle(radians,10); }
inline bool ReadPreference(std::istream& input,int& value) {
 std::string token;int candidate;
 if(!(input>>token))return false;
 if(token=="DWG2_FOV_V2") {if(!(input>>candidate)||!ValidPercent(candidate))return false;}
 else if(token=="0")candidate=0;
 else if(token=="1")candidate=10; // Preserve the previous Default/Extended setting.
 else return false;
 std::string extra;if(input>>extra)return false;
 value=candidate;return true;
}
}
