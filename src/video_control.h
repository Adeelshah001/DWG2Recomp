#pragma once
#include <atomic>
#include <istream>
#include <string>
namespace dwg2::video {
inline std::atomic<bool> vsync{false};
inline bool Read(std::istream& in,bool& value){std::string magic,extra;int n;if(!(in>>magic>>n)||magic!="DWG2_VIDEO_V1"||(n!=0&&n!=1)||(in>>extra))return false;value=n!=0;return true;}
}
