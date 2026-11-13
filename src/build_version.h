#pragma once
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
namespace dwg2::build_version {
inline bool Valid(const std::wstring& value){if(value.empty()||value.size()>64)return false;for(wchar_t c:value)if(!((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||c==L'.'||c==L'-'))return false;return value[0]>=L'0'&&value[0]<=L'9';}
inline std::wstring Read(const std::filesystem::path& root){wchar_t value[65]{};DWORD length=GetEnvironmentVariableW(L"DWG2_RELEASE_VERSION",value,65);if(length&&length<65&&Valid(value))return value;std::ifstream file(root/"installed.txt");std::string line;if(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::wstring version(line.begin(),line.end());if(Valid(version))return version;}return L"Unknown";}
inline std::filesystem::path Root(){wchar_t exe[32768]{};DWORD size=GetModuleFileNameW(nullptr,exe,32768);return size&&size<32768?std::filesystem::path(exe).parent_path().parent_path():std::filesystem::path{};}
inline std::wstring Label(const std::filesystem::path& root){auto version=Read(root);return L"Version: "+(version==L"Unknown"?version:L"v"+version);}
inline bool Append(HMENU menu,UINT id,const std::filesystem::path& root){auto text=Label(root);return AppendMenuW(menu,MF_STRING|MF_GRAYED,id,text.c_str())!=FALSE;}
}
