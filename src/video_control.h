#pragma once
#include <atomic>
#include <istream>
#include <string>
namespace dwg2::video {
inline constexpr int kDefaultFps = 60;
inline std::atomic<bool> vsync{false};
inline std::atomic<bool> fps_counter{false};
inline std::atomic<int> present_fps{kDefaultFps};
inline std::atomic<bool> present_fps_initialized{false};
inline std::atomic<bool> battle_paced{false};
inline constexpr bool ValidFps(int value) {
 return value==0||value==60||value==120;
}
inline constexpr int NormalizeFps(int value) {
 return ValidFps(value)?value:kDefaultFps;
}
inline constexpr int HostPacingFps(int fps, bool guest_paces) {
 return guest_paces?0:fps;
}
inline bool Read(std::istream& in,bool& value){std::string magic,extra;int n;if(!(in>>magic>>n)||magic!="DWG2_VIDEO_V1"||(n!=0&&n!=1)||(in>>extra))return false;value=n!=0;return true;}
}
