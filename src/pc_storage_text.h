#pragma once
#include <string>
namespace dwg2::storage {
inline std::string DisplayText(std::string text){if(text=="Xbox 360 Hard Drive"||text=="Xbox 360 Memory Unit")return "System Storage";if(text=="Checking the DEVICE...")return "Checking system storage...";for(const char* token:{"Xbox 360 Hard Drive","Xbox 360 Memory Unit"}){size_t at=0;while((at=text.find(token,at))!=std::string::npos){text.replace(at,std::char_traits<char>::length(token),"system storage");at+=14;}}const std::string prefix="Checking the system storage";if(text.starts_with(prefix))text.replace(0,prefix.size(),"Checking system storage");return text;}
}
