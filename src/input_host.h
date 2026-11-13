#pragma once
#include "input_config.h"
#include "fullscreen_edge.h"
#include "update_input_gate.h"
#include "player_control_settings.h"
#include "prompt_control.h"
#include <windows.h>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <map>
extern "C" int Dwg2NotifyInputActivity(int family,int controller);

namespace dwg2::input {
inline std::filesystem::path Root() {
 wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
 return std::filesystem::path(exe).parent_path().parent_path();
}
inline int Family(SDL_Gamepad* pad) {
 auto type=SDL_GetGamepadType(pad);
 return type==SDL_GAMEPAD_TYPE_PS5 || type==SDL_GAMEPAD_TYPE_PS4?1:2;
}
inline Raw ReadPad(SDL_Gamepad* pad) {
 Raw raw;
 for(int b=0;b<SDL_GAMEPAD_BUTTON_COUNT && b<26;++b)raw.buttons[b]=SDL_GetGamepadButton(pad,static_cast<SDL_GamepadButton>(b));
 for(int a=0;a<6;++a) {
  const int value=SDL_GetGamepadAxis(pad,static_cast<SDL_GamepadAxis>(a));
  raw.axes[a]=value/(value<0?32768.f:32767.f);
 }
 return raw;
}
inline std::mutex edges_mutex;
inline std::map<unsigned long long,bool> fullscreen_edges;
struct GateState { unsigned generation=0; bool wait=false; };
inline std::map<unsigned long long,GateState> release_gates;
inline std::map<unsigned long long,Raw> prompt_activity;
inline void ObservePromptDevice(unsigned long long id,int family,const Raw& raw) {
 std::lock_guard lock(edges_mutex);auto& previous=prompt_activity[id];bool changed=false;
 for(size_t i=0;i<raw.buttons.size();++i)changed|=raw.buttons[i]&&!previous.buttons[i];
 for(size_t i=0;i<raw.keys.size();++i)changed|=raw.keys[i]&&!previous.keys[i];
 for(size_t i=0;i<raw.axes.size();++i)
  changed|=std::abs(raw.axes[i])>.35f&&std::abs(raw.axes[i]-previous.axes[i])>.12f;
 previous=raw;if(changed&&!editing.load()) {
  Dwg2NotifyInputActivity(family,id!=0x100000000ull?1:0);
 }
}
inline bool Gate(unsigned long long device,bool held) {
 std::lock_guard lock(edges_mutex);auto& gate=release_gates[device];const auto current=generation.load();
 if(gate.generation && gate.generation!=current)gate.wait=true;
 gate.generation=current;
 return dwg2::update_modal::GateUntilReleased(editing.load()||dwg2::update_modal::Active(),held,gate.wait);
}
inline void Shortcut(unsigned long long device,float level,HWND game) {
 std::lock_guard lock(edges_mutex);bool& previous=fullscreen_edges[device];
 if(FullscreenEdge(level,previous) && !editing.load() && game && GetForegroundWindow()==game)
  PostMessageW(game,WM_APP+0x212,0,0);

}
template<class Gamepad> inline void Pad(SDL_Gamepad* device,Gamepad& out,HWND window,bool active) {
 Output result;
 const auto id=static_cast<unsigned long long>(SDL_GetGamepadID(device));
 if(SDL_GamepadConnected(device)) {
  auto raw=ReadPad(device);bool held=false;for(bool button:raw.buttons)held|=button;
  for(float axis:raw.axes)held|=std::abs(axis)>.25f;
  if(!Gate(id,held) && active) {
   result=Map(Get(Family(device)),raw);
   ObservePromptDevice(id,Family(device)==1?2:1,raw);
  }
 }
 Write(result,out);Shortcut(id,result.fullscreen_level,window);
}
template<class Gamepad> inline void Keyboard(const bool (&keys)[256],Gamepad& out,HWND window) {
 Raw raw;std::copy(std::begin(keys),std::end(keys),raw.keys.begin());
 raw.mods=(keys[16]?1:0)|(keys[17]?2:0)|(keys[18]?4:0);
 bool held=false;for(bool key:raw.keys)held|=key;
 const bool blocked=Gate(0x100000000ull,held);
 Output result;if(!blocked) {
  result=Map(Get(0),raw);ObservePromptDevice(0x100000000ull,3,raw);
 }
 if(pause_pressed.load() && !blocked)result.buttons|=0x10;
 Write(result,out);Shortcut(0x100000000ull,(raw.keys[13]&&(raw.mods&4))?0.f:result.fullscreen_level,window);
}
inline bool Load() {
 dwg2::settings::Document doc;if(!dwg2::settings::Read(dwg2::settings::Path(),doc))return false;
 auto result=profiles;if(!LoadPlayerControls(doc,result))return false;
 std::lock_guard lock(mutex);profiles=result;generation.fetch_add(1);return true;
}
inline bool Save(const std::array<Profile,3>& data) {return SavePlayerControls(data);}

}
