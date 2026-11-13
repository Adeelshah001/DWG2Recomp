// Shared player INI storage. Preserve other sections and comments on each edit.
#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cwctype>
namespace dwg2::settings {
inline std::string Trim(std::string s) {auto a=s.find_first_not_of(" \t\r");if(a==s.npos)return {};return s.substr(a,s.find_last_not_of(" \t\r")-a+1);}
inline std::string Lower(std::string s){for(auto&c:s)if(c>='A'&&c<='Z')c+=32;return s;}
inline std::filesystem::path Path(){wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);return std::filesystem::path(exe).parent_path().parent_path()/L"settings.ini";}
struct Document {
 std::vector<std::string> lines;std::map<std::string,std::string> values;
 bool Parse(const std::string& text){lines.clear();values.clear();if(text.size()>1024*1024)return false;std::istringstream in(text);std::string line,section;bool first=true;
  while(std::getline(in,line)){if(first&&line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);first=false;if(!line.empty()&&line.back()=='\r')line.pop_back();lines.push_back(line);auto s=Trim(line);if(s.empty()||s[0]==';'||s[0]=='#')continue;
   if(s[0]=='['){if(s.back()!=']')return false;section=Lower(Trim(s.substr(1,s.size()-2)));if(section.empty())return false;continue;}
   auto eq=s.find('=');if(eq==s.npos||section.empty())return false;auto key=Lower(Trim(s.substr(0,eq)));auto value=Trim(s.substr(eq+1));auto comment=value.find_first_of(";#");if(comment!=value.npos)value=Trim(value.substr(0,comment));if(key.empty()||!values.emplace(section+"/"+key,value).second)return false;
  }return true;
 }
 std::string Get(std::string section,std::string key,std::string fallback={})const{auto i=values.find(Lower(section)+"/"+Lower(key));return i==values.end()?fallback:i->second;}
 void Set(std::string section,std::string key,const std::string& value){section=Lower(section);key=Lower(key);std::string current;size_t insertion=lines.size();bool found=false;
  for(size_t i=0;i<lines.size();++i){auto s=Trim(lines[i]);if(s.starts_with("[")&&s.ends_with("]")){if(found){insertion=i;break;}current=Lower(Trim(s.substr(1,s.size()-2)));if(current==section){found=true;insertion=i+1;}continue;}
   if(current==section){insertion=i+1;auto eq=s.find('=');if(eq!=s.npos&&Lower(Trim(s.substr(0,eq)))==key){auto comment=s.find_first_of(";#",eq+1);lines[i]=key+" = "+value+(comment==s.npos?"":" "+s.substr(comment));values[section+"/"+key]=value;return;}}
  }
  if(!found){if(!lines.empty()&&!lines.back().empty())lines.push_back("");lines.push_back("["+section+"]");insertion=lines.size();}lines.insert(lines.begin()+insertion,key+" = "+value);values[section+"/"+key]=value;
 }
 std::string Text()const{std::string text;for(const auto&line:lines)text+=line+'\n';return text;}
};
inline bool Read(const std::filesystem::path& path,Document& doc){std::error_code ec;if(!std::filesystem::exists(path,ec))return !ec&&doc.Parse("");if(std::filesystem::file_size(path,ec)>1024*1024||ec)return false;std::ifstream in(path,std::ios::binary);if(!in)return false;std::string s((std::istreambuf_iterator<char>(in)),{});return !in.bad()&&doc.Parse(s);}
inline bool Number(const std::string& text,int& result){std::istringstream in(text);int n;if(!(in>>n))return false;in>>std::ws;if(!in.eof())return false;result=n;return true;}
inline int Integer(const Document& doc,const char* section,const char* key,int fallback,int lo,int hi){int n;return Number(doc.Get(section,key),n)&&n>=lo&&n<=hi?n:fallback;}
struct Lock {HANDLE handle=nullptr;bool held=false;explicit Lock(const std::filesystem::path& path){auto name=std::filesystem::absolute(path).lexically_normal().wstring();unsigned long long hash=14695981039346656037ull;for(wchar_t c:name){hash^=static_cast<unsigned short>(std::towlower(c));hash*=1099511628211ull;}name=L"Local\\DWG2Settings_"+std::to_wstring(hash);handle=CreateMutexW(nullptr,FALSE,name.c_str());if(handle){auto r=WaitForSingleObject(handle,5000);held=r==WAIT_OBJECT_0||r==WAIT_ABANDONED;}}~Lock(){if(held)ReleaseMutex(handle);if(handle)CloseHandle(handle);}};
inline bool Update(const std::filesystem::path& path,const std::map<std::string,std::string>& changes){Lock lock(path);if(!lock.held)return false;Document doc;if(!Read(path,doc))return false;for(auto&[key,value]:changes){auto slash=key.find('/');if(slash==key.npos)return false;doc.Set(key.substr(0,slash),key.substr(slash+1),value);}auto temp=path;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<doc.Text();out.flush();if(!out){std::error_code ec;std::filesystem::remove(temp,ec);return false;}}if(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;std::error_code ec;std::filesystem::remove(temp,ec);return false;}
inline bool Set(const char* section,const char* key,int value){return Update(Path(),{{std::string(section)+"/"+key,std::to_string(value)}});}
}
