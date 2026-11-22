#pragma once
#include <atomic>
namespace dwg2::resolution {
// Existing integer preferences remain valid.15 identifies the rational3/2 profile.
inline bool Valid(int code) {return code==1||code==15||code==2||code==3;}
inline int Numerator(int code) {return code==15?3:code;}
inline int Denominator(int code) {return code==15?2:1;}
inline int Width(int code) {return 1280*Numerator(code)/Denominator(code);}
inline int Height(int code) {return 720*Numerator(code)/Denominator(code);}
inline std::atomic<int> requested{0}, completed{0}, error{0};
}
