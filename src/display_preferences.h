#pragma once
#include "player_settings.h"
namespace dwg2::display {
inline int Fullscreen(const dwg2::settings::Document& doc){return dwg2::settings::Integer(doc,"video","fullscreen",0,0,1);}
template<class Apply> bool SetFullscreen(const std::filesystem::path& path,bool value,Apply apply){if(!dwg2::settings::Update(path,{{"video/fullscreen",value?"1":"0"}}))return false;apply(value);return true;}
}
