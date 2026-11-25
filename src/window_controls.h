// Original DWG2 host UI. No game code or assets.
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <filesystem>
#include <map>
#include <rex/ui/window_sdl.h>
#include "prompt_control.h"
extern "C" __declspec(dllexport) int Dwg2SeedConnectedController(int style) {
  dwg2::prompts::SeedConnectedController(style);
  return dwg2::prompts::Effective();
}
#include "input_ui.h"
#include "recomp_menu_state.h"
#include "build_version.h"
#include "player_settings.h"
#include "display_preferences.h"
#include "resolution_control.h"
#include "camera_control.h"
#include "video_control.h"
#include "fov_ui.h"
#include "runtime_pause.h"
#include <rex/chrono/clock.h>
#include <rex/logging.h>
#include <rex/cvar.h>
REXCVAR_DECLARE(bool,dwg2_host_vsync);
REXCVAR_DECLARE(int32_t,dwg2_present_fps);

namespace dwg2::controls {
inline std::atomic<HWND> graphics_window{nullptr};
inline std::atomic<int> graphics_scale{1},graphics_fullscreen{0},graphics_pending{0};
extern "C" __declspec(dllexport) int Dwg2GraphicsValue(int key) {
  if(key==0)return graphics_scale.load();
  if(key==1)return dwg2::camera::percent.load();
  if(key==2)return graphics_fullscreen.load();
  if(key==3)return graphics_pending.load();
  if(key==4)return dwg2::video::vsync.load()?1:0;
  if(key==5)return dwg2::video::present_fps.load();
  if(key==6)return dwg2::video::fps_counter.load()?1:0;
  if(key==7)return dwg2::video::turbo.load()?1:0;
  return -1;
}
constexpr UINT kScale1 = 41011, kScale3 = 41013;
inline UINT ScaleCommand(int code) {return kScale1+code-1;}
extern "C" __declspec(dllexport) int Dwg2GraphicsRequest(int key,int value) {
  const HWND hwnd=graphics_window.load();if(!hwnd)return 0;
  if(key==0&&(value>=1&&value<=3)) {
    if(graphics_pending.load())return 0;
    return PostMessageW(hwnd,WM_COMMAND,ScaleCommand(value),0)!=0;
  }
  if(key==1&&dwg2::camera::ValidPercent(value))return PostMessageW(hwnd,WM_APP+0x213,value,0)!=0;
  if(key==2&&(value==0||value==1))return PostMessageW(hwnd,WM_APP+0x214,value,0)!=0;
  if(key==4&&(value==0||value==1))return PostMessageW(hwnd,WM_APP+0x215,value,0)!=0;
  if(key==5&&dwg2::video::ValidFps(value))return PostMessageW(hwnd,WM_APP+0x216,value,0)!=0;
  if(key==6&&(value==0||value==1))return PostMessageW(hwnd,WM_APP+0x217,value,0)!=0;
  if(key==7&&(value==0||value==1))return PostMessageW(hwnd,WM_APP+0x21B,value,0)!=0;
  return 0;
}
extern "C" __declspec(dllexport) int Dwg2SetGpuVsync(int enabled) {
  return rex::cvar::SetFlagByName("vsync",enabled?"true":"false")?1:0;
}
extern "C" __declspec(dllexport) int Dwg2SetBattlePacing(int active, int gpu_vsync) {
  dwg2::video::battle_paced.store(active != 0);
  return Dwg2SetGpuVsync(gpu_vsync);
}
constexpr UINT_PTR kNativePump = 0x44574732;
constexpr UINT_PTR kResolutionTimer = 0x44574733;
struct State {
  rex::ui::WindowSDL* owner;
  SDL_Window* sdl;
  int pending_scale = 0;
  bool native_loop = false;
  HWND update_prompt = nullptr;
  ULONGLONG update_tick = 0;
  unsigned update_released = 0;
  bool update_armed = false;
  DWORD launcher_pid = 0;
  bool update_background = false;
  std::string update_previous_background;
  bool update_had_background = false;
  bool prev_kb_menu_toggle = false;
  std::map<unsigned long long, bool> prev_gp_menu_toggle;
  int last_gp_family = 2;
  bool was_menu_open = false;
  std::string cached_version;
};
// Poll the same standardized SDL devices as the game while its launcher prompt
// owns foreground focus. No guest input mapping or audio state is changed.
inline void PollUpdatePrompt(State* state) {
  const auto now=GetTickCount64();if(now-state->update_tick<30)return;state->update_tick=now;
  HWND prompt=GetForegroundWindow();DWORD pid=0;if(prompt)GetWindowThreadProcessId(prompt,&pid);
  if(!state->launcher_pid||pid!=state->launcher_pid||!GetPropW(prompt,L"DWG2.UpdatePrompt")) {
    if(state->update_background){SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,state->update_had_background?state->update_previous_background.c_str():nullptr);state->update_background=false;}
    state->update_prompt=nullptr;state->update_armed=false;state->update_released=0;return;
  }
  if(prompt!=state->update_prompt){state->update_prompt=prompt;state->update_armed=false;state->update_released=0;}
  if(!state->update_background){const char* previous=SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);state->update_had_background=previous!=nullptr;state->update_previous_background=previous?previous:"";SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");state->update_background=true;}
  SDL_UpdateGamepads();bool yes=false,no=false;int count=0;auto* ids=SDL_GetGamepads(&count);
  if(ids){for(int i=0;i<count&&i<16;++i){auto* pad=SDL_OpenGamepad(ids[i]);if(!pad)continue;
    yes|=SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_SOUTH);no|=SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_EAST);SDL_CloseGamepad(pad);
  }SDL_free(ids);}
  if(!state->update_armed){if(!yes&&!no){if(++state->update_released>=7)state->update_armed=true;}else state->update_released=0;return;}
  if(no||yes){PostMessageW(prompt,0x8319,no?2:1,0);state->update_armed=false;state->update_released=0;}
}
inline const std::filesystem::path& SettingsPath() {
  return dwg2::settings::Path();
}
inline void Sync(rex::ui::WindowSDL* owner) {
  graphics_fullscreen.store(owner->IsFullscreen()?1:0);
}
inline bool SavePrompts(int choice,bool hud) {
 return dwg2::settings::Update(SettingsPath(),{{"prompts/style",std::to_string(choice)},{"prompts/hud_correction",std::to_string(int(hud))}});
}
inline bool ApplyFullscreen(State* state,bool value,bool show_error=true) {
 if(dwg2::display::SetFullscreen(SettingsPath(),value,[&](bool mode){state->owner->SetFullscreen(mode);Sync(state->owner);}))return true;
 if(show_error)MessageBoxW(static_cast<HWND>(state->owner->GetNativeWindowHandle()),L"Fullscreen could not be saved. The previous setting is retained.",L"Graphics / Video",MB_ICONERROR);
 return false;
}
inline void SetFullscreen(State* state,bool value) {
 ApplyFullscreen(state,value,true);
}
inline bool ApplyPresentFps(HWND hwnd,rex::ui::WindowSDL* owner,int fps,bool show_error=true) {
  if(!dwg2::video::ValidFps(fps))return false;
  if(dwg2::settings::Set("video","presentation_fps",fps)) {
    dwg2::video::present_fps.store(fps);
    dwg2::video::present_fps_initialized.store(true);
    Sync(owner);
    REXLOG_INFO("DWG2 presentation_fps set: {}",fps);
    return true;
  }
  if(show_error)MessageBoxW(hwnd,L"The FPS setting could not be saved. The previous value is retained.",L"Frame Rate",MB_ICONERROR);
  Sync(owner);
  return false;
}
inline void SetPresentFps(HWND hwnd,rex::ui::WindowSDL* owner,int fps) {
  ApplyPresentFps(hwnd,owner,fps,true);
}
inline bool ApplyFpsCounter(HWND hwnd,rex::ui::WindowSDL* owner,bool enabled,bool show_error=true) {
  if(dwg2::settings::Set("video","fps_counter",int(enabled))) {
    dwg2::video::fps_counter.store(enabled);
    Sync(owner);
    REXLOG_INFO("DWG2 fps_counter set: {}",enabled);
    return true;
  }
  if(show_error)MessageBoxW(hwnd,L"The FPS counter setting could not be saved. The previous value is retained.",L"FPS Counter",MB_ICONERROR);
  Sync(owner);
  return false;
}
inline void SetFpsCounter(HWND hwnd,rex::ui::WindowSDL* owner,bool enabled) {
  ApplyFpsCounter(hwnd,owner,enabled,true);
}
inline bool ApplyVsync(HWND hwnd,bool enabled,bool show_error=true) {
  if(dwg2::settings::Set("video","vsync",int(enabled))){dwg2::video::vsync.store(enabled);return true;}
  if(show_error)MessageBoxW(hwnd,L"The VSync setting could not be saved. The previous value is retained.",L"Graphics / Video",MB_ICONERROR);
  return false;
}
inline bool ApplyTurbo(HWND hwnd,rex::ui::WindowSDL* owner,bool enabled,bool show_error=true) {
  if(dwg2::settings::Set("video","turbo",int(enabled))) {
    dwg2::video::turbo.store(enabled);
    Sync(owner);
    REXLOG_INFO("DWG2 turbo set: {}",enabled);
    return true;
  }
  if(show_error)MessageBoxW(hwnd,L"The Turbo Mode setting could not be saved. The previous value is retained.",L"Turbo Mode",MB_ICONERROR);
  Sync(owner);
  return false;
}
inline bool ApplyFov(HWND hwnd,int value,bool show_error=true) {
  if(!dwg2::camera::ValidPercent(value))return false;
  if(dwg2::camera::SavePreference(SettingsPath(),value)){dwg2::camera::percent.store(value);dwg2::camera::SyncEditor(value);return true;}
  if(show_error)MessageBoxW(hwnd,L"The FOV setting could not be saved. The previous value is retained.",L"Field of view",MB_ICONERROR);
  return false;
}
inline bool ApplyPrompts(HWND hwnd,rex::ui::WindowSDL* owner,int choice,bool hud,bool show_error=true) {
  if(choice<0||choice>dwg2::prompts::kMaxStyle)return false;
  if(!SavePrompts(choice,hud)){if(show_error)MessageBoxW(hwnd,L"Could not save prompt/display settings. Your previous settings are retained.",L"DWG2",MB_ICONERROR);return false;}
  dwg2::prompts::choice.store(choice);dwg2::prompts::hud_correction.store(hud);Sync(owner);return true;
}
inline bool TogglePause(State* s) {
  const bool paused=!dwg2::pause::Requested();
  dwg2::pause::Set(paused,rex::chrono::Clock::QueryHostTickCount());
  Sync(s->owner);
  REXLOG_INFO("DWG2 explicit runtime pause: {}",paused);
  return paused;
}
inline void RequestResolutionScale(HWND hwnd,State* s,int scale) {
  if(scale<1||scale>3)return;
  s->pending_scale=scale;
  graphics_pending.store(scale);
  dwg2::resolution::completed.store(0);
  dwg2::resolution::error.store(0);
  dwg2::resolution::requested.store(scale);
  SetTimer(hwnd, kResolutionTimer, 50, nullptr);
}
inline std::string RequestRestart(HWND hwnd) {
  wchar_t name[128]{};
  DWORD length=GetEnvironmentVariableW(L"DWG2_RESTART_EVENT",name,128);
  HANDLE event=(length&&length<128)?OpenEventW(EVENT_MODIFY_STATE,FALSE,name):nullptr;
  if(!event) {
    return "Start via DWG2R.exe to use Restart.";
  }
  BOOL requested=SetEvent(event);
  if(requested&&!PostMessageW(hwnd,WM_CLOSE,0,0)){ResetEvent(event);requested=FALSE;}
  CloseHandle(event);
  if(!requested) {
    return "Restart could not be requested.";
  }
  return {};
}
inline std::string RequestCheckUpdates() {
  wchar_t name[128]{};DWORD length=GetEnvironmentVariableW(L"DWG2_UPDATE_EVENT",name,128);
  HANDLE event=(length&&length<128)?OpenEventW(EVENT_MODIFY_STATE,FALSE,name):nullptr;
  if(!event) {
    return "Start via DWG2R.exe to check for updates.";
  }
  const BOOL ok=SetEvent(event);CloseHandle(event);
  if(!ok) {
    return "Update request could not be sent.";
  }
  return "Checking for updates...";
}
inline dwg2::recomp_menu::Actions BuildMenuActions(HWND hwnd,State* s) {
  dwg2::recomp_menu::Actions act;
  act.restart=[hwnd]{return RequestRestart(hwnd);};
  act.toggle_pause=[s]{TogglePause(s);return std::string{};};
  act.quit_game=[hwnd]{PostMessageW(hwnd,WM_CLOSE,0,0);};
  act.set_fullscreen=[s](bool v){return ApplyFullscreen(s,v,false)?std::string{}:std::string("Could not save fullscreen setting.");};
  act.set_resolution=[hwnd,s](int scale){if(graphics_pending.load())return std::string("Resolution change pending.");RequestResolutionScale(hwnd,s,scale);return std::string{};};
  act.set_fov=[hwnd](int v){return ApplyFov(hwnd,v,false)?std::string{}:std::string("Could not save FOV setting.");};
  act.set_fps=[hwnd,s](int fps){return ApplyPresentFps(hwnd,s->owner,fps,false)?std::string{}:std::string("Could not save FPS setting.");};
  act.set_fps_counter=[hwnd,s](bool on){return ApplyFpsCounter(hwnd,s->owner,on,false)?std::string{}:std::string("Could not save FPS counter.");};
  act.set_vsync=[hwnd](bool on){return ApplyVsync(hwnd,on,false)?std::string{}:std::string("Could not save VSync setting.");};
  act.set_turbo=[hwnd,s](bool on){return ApplyTurbo(hwnd,s->owner,on,false)?std::string{}:std::string("Could not save Turbo Mode.");};
  act.set_prompts=[hwnd,s](int c){return ApplyPrompts(hwnd,s->owner,c,dwg2::prompts::hud_correction.load(),false)?std::string{}:std::string("Could not save prompt style.");};
  act.check_updates=[]{return RequestCheckUpdates();};
  act.save_controls=[](const std::array<dwg2::input::Profile,3>& p){return dwg2::input::SavePlayerControls(p)?std::string{}:std::string("Could not save settings.ini.");};
  act.is_paused=[]{return dwg2::pause::Requested();};
  act.is_fullscreen=[s]{return s->owner->IsFullscreen();};
  act.resolution=[]{return graphics_scale.load();};
  act.resolution_pending=[]{return graphics_pending.load();};
  act.fov=[]{return dwg2::camera::percent.load();};
  act.fps=[]{return dwg2::video::present_fps.load();};
  act.fps_counter=[]{return dwg2::video::fps_counter.load();};
  act.vsync=[]{return dwg2::video::vsync.load();};
  act.turbo=[]{return dwg2::video::turbo.load();};
  act.prompt_choice=[]{return dwg2::prompts::choice.load();};
  act.prompt_style=[]{return dwg2::prompts::Effective();};
  act.version_text=[s]{return s->cached_version;};
  return act;
}
inline void RequestRecompMenuPaint(State* s) {
  const bool queued = s->owner->RequestForcedPaintFromUIThread();
  s->was_menu_open = dwg2::recomp_menu::IsOpen() || !queued;
}
inline void PollRecompMenu(HWND hwnd,State* s) {
  if(GetForegroundWindow()!=hwnd||dwg2::input::editing.load()||dwg2::update_modal::Active()) {
    s->prev_kb_menu_toggle=false;
    for(auto& kv:s->prev_gp_menu_toggle)kv.second=false;
    if(s->was_menu_open&&!dwg2::recomp_menu::IsOpen())RequestRecompMenuPaint(s);
    return;
  }
  dwg2::input::Raw kb_raw;
  for(int k=8;k<256;++k)kb_raw.keys[k]=(GetAsyncKeyState(k)&0x8000)!=0;
  kb_raw.mods=(kb_raw.keys[VK_SHIFT]?1:0)|(kb_raw.keys[VK_CONTROL]?2:0)|(kb_raw.keys[VK_MENU]?4:0);
  const auto kb_prof=dwg2::input::Get(0);
  const bool kb_toggle=dwg2::input::Level(kb_prof.bind[dwg2::input::RecompMenu],kb_raw,0)>=0.65f||
                       dwg2::input::Level(kb_prof.alternate[dwg2::input::RecompMenu],kb_raw,0)>=0.65f;

  SDL_UpdateGamepads();
  int count=0;auto* ids=SDL_GetGamepads(&count);
  bool has_active_gp=false;
  dwg2::input::Raw active_gp_raw{};
  int active_gp_style=dwg2::prompts::kXbox360;
  unsigned long long active_gp_id=0;
  bool gp_open_trigger=false;
  dwg2::input::Raw trigger_gp_raw{};
  int trigger_gp_style=dwg2::prompts::kXbox360;
  unsigned long long trigger_gp_id=0;
  if(ids) {
    for(int i=0;i<count&&i<16;++i) {
      auto* pad=SDL_OpenGamepad(ids[i]);if(!pad)continue;
      const auto id=static_cast<unsigned long long>(SDL_GetGamepadID(pad));
      const int family=dwg2::input::Family(pad);
      const int style=dwg2::input::PromptStyle(pad);
      if(SDL_GamepadConnected(pad))dwg2::prompts::SeedConnectedController(style);
      const auto raw=dwg2::input::ReadPad(pad);
      SDL_CloseGamepad(pad);
      const auto gp_prof=dwg2::input::Get(family);
      const bool gp_toggle=dwg2::input::Level(gp_prof.bind[dwg2::input::RecompMenu],raw,gp_prof.deadzone)>=0.65f||
                           dwg2::input::Level(gp_prof.alternate[dwg2::input::RecompMenu],raw,gp_prof.deadzone)>=0.65f;
      bool& prev=s->prev_gp_menu_toggle[id];
      if(gp_toggle&&!prev&&!gp_open_trigger) {
        gp_open_trigger=true;trigger_gp_raw=raw;trigger_gp_style=style;trigger_gp_id=id;
      }
      prev=gp_toggle;
      bool held=false;
      for(bool b:raw.buttons)held|=b;
      for(float a:raw.axes)held|=std::abs(a)>0.25f;
      if(held&&!has_active_gp) {
        has_active_gp=true;active_gp_raw=raw;active_gp_style=style;active_gp_id=id;s->last_gp_family=family;
      }
    }
    SDL_free(ids);
  }

  const bool was_open=dwg2::recomp_menu::IsOpen();
  if(!was_open) {
    if(kb_toggle&&!s->prev_kb_menu_toggle) {
      dwg2::input::ObservePromptDevice(0x100000000ull,3,kb_raw);
      dwg2::recomp_menu::SetOpen(true,true);
    } else if(gp_open_trigger) {
      dwg2::input::ObservePromptDevice(trigger_gp_id,trigger_gp_style,trigger_gp_raw);
      dwg2::recomp_menu::SetOpen(true,true);
    }
  } else {
    dwg2::input::ObservePromptDevice(0x100000000ull,3,kb_raw);
    if(has_active_gp)dwg2::input::ObservePromptDevice(active_gp_id,active_gp_style,active_gp_raw);
    const auto act=BuildMenuActions(hwnd,s);
    const uint64_t now_ms=GetTickCount64();
    dwg2::recomp_menu::HandleKeyboard(kb_raw,now_ms,act);
    if(dwg2::recomp_menu::IsOpen()) {
      dwg2::recomp_menu::HandleGamepad(has_active_gp?active_gp_raw:dwg2::input::Raw{},s->last_gp_family,now_ms,act);
    }
  }
  s->prev_kb_menu_toggle=kb_toggle;
  const bool now_open=dwg2::recomp_menu::IsOpen();
  if(!now_open&&GetCapture()==hwnd)ReleaseCapture();
  if(now_open||was_open||s->was_menu_open)RequestRecompMenuPaint(s);
}
inline LRESULT CALLBACK Procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                   UINT_PTR id, DWORD_PTR data) {
  auto* s = reinterpret_cast<State*>(data);
  if(msg==WM_APP+0x213) {
    ApplyFov(hwnd,int(wp),true);
    return 0;
  }
  if(msg==WM_APP+0x214) {
    if(wp<=1){SetFullscreen(s,wp!=0);}return 0;
  }
  if(msg==WM_APP+0x215) {
    if(wp<=1)ApplyVsync(hwnd,wp!=0,true);
    return 0;
  }
  if(msg==WM_APP+0x216) {
    SetPresentFps(hwnd,s->owner,int(wp));
    return 0;
  }
  if(msg==WM_APP+0x217) {
    if(wp<=1)SetFpsCounter(hwnd,s->owner,wp!=0);
    return 0;
  }
  if(msg==WM_APP+0x218) {
    dwg2::recomp_menu::Toggle();
    RequestRecompMenuPaint(s);
    return 0;
  }
  if(msg==WM_APP+0x219) {
    if(wp<=dwg2::prompts::kMaxStyle){ApplyPrompts(hwnd,s->owner,int(wp),dwg2::prompts::hud_correction.load(),false);RequestRecompMenuPaint(s);}
    return 0;
  }
  if(msg==WM_APP+0x21A) {
    TogglePause(s);
    RequestRecompMenuPaint(s);
    return 0;
  }
  if(msg==WM_APP+0x21B) {
    if(wp<=1)ApplyTurbo(hwnd,s->owner,wp!=0,true);
    return 0;
  }
  if(msg==WM_SIZE)dwg2::recomp_menu::InvalidateHitRects();
  if(msg==WM_CAPTURECHANGED)dwg2::recomp_menu::HandleMouseUp();
  if(dwg2::recomp_menu::IsOpen()&&(msg==WM_MOUSEMOVE||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_MOUSEWHEEL)) {
    if(msg==WM_MOUSEWHEEL) {
      dwg2::recomp_menu::HandleMouseWheel(float(static_cast<short>(HIWORD(wp)))/120.0f);
      RequestRecompMenuPaint(s);
      return 0;
    }
    RECT rc{};GetClientRect(hwnd,&rc);
    float vx=0.f,vy=0.f;
    dwg2::recomp_menu::ScreenToVirtual(float(static_cast<short>(LOWORD(lp))),float(static_cast<short>(HIWORD(lp))),float(rc.right-rc.left),float(rc.bottom-rc.top),vx,vy);
    if(msg==WM_MOUSEMOVE)dwg2::recomp_menu::HandleMouseMove(vx,vy,BuildMenuActions(hwnd,s));
    else if(msg==WM_LBUTTONDOWN) {
      if (rc.right > 1 && rc.bottom > 1 &&
          dwg2::recomp_menu::HandleMouseDown(vx,vy,BuildMenuActions(hwnd,s),GetTickCount64()) &&
          dwg2::recomp_menu::IsOpen()) SetCapture(hwnd);
    }
    else{if(GetCapture()==hwnd)ReleaseCapture();dwg2::recomp_menu::HandleMouseUp();}
    if(!dwg2::recomp_menu::IsOpen()&&GetCapture()==hwnd)ReleaseCapture();
    RequestRecompMenuPaint(s);
    return 0;
  }
  if (msg == WM_APP + 0x212) {
    SetFullscreen(s,!s->owner->IsFullscreen()); return 0;
  }
  if (msg == WM_COMMAND && HIWORD(wp) == 0) {
    const UINT command = LOWORD(wp);
    if (command >= kScale1 && command <= kScale3) {
      RequestResolutionScale(hwnd,s,int(command - kScale1 + 1));
      return 0;
    }
  }
  if(msg == WM_TIMER && wp == kResolutionTimer) {
      const int scale=dwg2::resolution::completed.exchange(0);
      const int error=dwg2::resolution::error.exchange(0);
      if(error && error==s->pending_scale) {
        graphics_pending.store(0);
        KillTimer(hwnd,kResolutionTimer);
        MessageBoxW(hwnd,L"The renderer could not apply this resolution. Your previous resolution is still active.",L"Resolution",MB_ICONERROR);
        return 0;
      }
      if(!scale || scale!=s->pending_scale)return 0;
      KillTimer(hwnd,kResolutionTimer);
      s->pending_scale=0;
      graphics_scale.store(scale);graphics_pending.store(0);
      if (!dwg2::settings::Set("graphics","resolution_scale",scale)) {
        MessageBoxW(hwnd, L"Could not save the display setting. Check that the game folder is writable.", L"DWG2", MB_OK | MB_ICONERROR);
        return 0;
      }
      if(!s->owner->IsFullscreen()) {
        SDL_RestoreWindow(s->sdl);
        SDL_SetWindowSize(s->sdl,dwg2::resolution::Width(scale),dwg2::resolution::Height(scale));
      }
      return 0;
  }
  if(msg==WM_ENTERSIZEMOVE)s->native_loop=true;
  if(msg==WM_EXITSIZEMOVE)s->native_loop=false;
  if(msg==WM_TIMER && wp==kNativePump) {
    PollUpdatePrompt(s);
    PollRecompMenu(hwnd,s);
    if(s->native_loop)
      static_cast<rex::ui::SDLWindowedAppContext&>(s->owner->app_context()).PumpNativeLoop();
    return 0;
  }
  if(msg==WM_CLOSE)dwg2::pause::Shutdown(rex::chrono::Clock::QueryHostTickCount());
  if (msg == WM_SYSKEYDOWN && wp == VK_RETURN && !(lp & (1LL << 30))) {
    SetFullscreen(s,!s->owner->IsFullscreen()); return 0;
  }
  if (msg == WM_NCDESTROY) {
    dwg2::recomp_menu::SetOpen(false);
    graphics_window.store(nullptr);graphics_pending.store(0);
    if(s->update_background)SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,s->update_had_background?s->update_previous_background.c_str():nullptr);
    KillTimer(hwnd, kNativePump);
    KillTimer(hwnd, kResolutionTimer);
    dwg2::input::pause_pressed.store(false);
    if(dwg2::input::editor_window)DestroyWindow(dwg2::input::editor_window);
    if(dwg2::camera::editor)DestroyWindow(dwg2::camera::editor);
    RemovePropW(hwnd, L"DWG2.Controls");
    RemoveWindowSubclass(hwnd, Procedure, id);
    delete s;
  }
  return DefSubclassProc(hwnd, msg, wp, lp);
}
inline void Install(rex::ui::WindowSDL* owner, SDL_Window* sdl) {
  HWND hwnd = static_cast<HWND>(owner->GetNativeWindowHandle());
  if (!hwnd || GetPropW(hwnd, L"DWG2.Controls")) return;
  auto* s = new State{};
  s->owner = owner;
  s->sdl = sdl;
  {const auto wver=dwg2::build_version::Read(dwg2::build_version::Root());for(wchar_t c:wver)s->cached_version.push_back((c>=0x20&&c<=0x7E)?char(c):'?');}
  {wchar_t text[32]{};if(GetEnvironmentVariableW(L"DWG2_LAUNCHER_PID",text,32))s->launcher_pid=wcstoul(text,nullptr,10);}
  if (!SetPropW(hwnd, L"DWG2.Controls", s) || !SetWindowSubclass(hwnd, Procedure, 0x44574732, reinterpret_cast<DWORD_PTR>(s))) {
    RemovePropW(hwnd, L"DWG2.Controls"); delete s; return;
  }
  SetTimer(hwnd,kNativePump,8,nullptr);
  graphics_window.store(hwnd);
  dwg2::settings::Document preferences;
  if(!dwg2::settings::Read(SettingsPath(),preferences))MessageBoxW(hwnd,L"settings.ini could not be read. Defaults remain active; the file was preserved.",L"Settings",MB_ICONWARNING);
  owner->SetFullscreen(dwg2::display::Fullscreen(preferences)!=0);
  {int value=dwg2::settings::Integer(preferences,"camera","fov_percent",0,0,100);if(dwg2::camera::ValidPercent(value))dwg2::camera::percent.store(value);}
  dwg2::video::vsync.store(dwg2::settings::Integer(preferences,"video","vsync",REXCVAR_GET(dwg2_host_vsync)?1:0,0,1)!=0);
  REXCVAR_GET(dwg2_host_vsync)=dwg2::video::vsync.load();
  dwg2::video::fps_counter.store(dwg2::settings::Integer(preferences,"video","fps_counter",0,0,1)!=0);
  dwg2::video::turbo.store(dwg2::settings::Integer(preferences,"video","turbo",0,0,1)!=0);
  {const int fallback_fps=dwg2::video::NormalizeFps(REXCVAR_GET(dwg2_present_fps));
   const int fps_val=dwg2::video::NormalizeFps(dwg2::settings::Integer(preferences,"video","presentation_fps",fallback_fps,0,240));
   dwg2::video::present_fps.store(fps_val);
   dwg2::video::present_fps_initialized.store(true);}
  dwg2::prompts::choice.store(dwg2::settings::Integer(preferences,"prompts","style",0,0,dwg2::prompts::kMaxStyle));
  dwg2::prompts::hud_correction.store(dwg2::settings::Integer(preferences,"prompts","hud_correction",1,0,1)!=0);
  Sync(owner);
  {int scale=dwg2::settings::Integer(preferences,"graphics","resolution_scale",1,1,3);if(scale<1||scale>3)scale=1;graphics_scale.store(scale);}
  if(!dwg2::input::Load())MessageBoxW(hwnd,L"settings.ini could not be read. Default mappings are active; the file is preserved until you save new mappings.",L"Controls",MB_ICONWARNING);
  REXLOG_INFO("DWG2 settings.ini loaded: scale={} fov={} vsync={} fps={} fps_counter={} turbo={} prompts={} hud_correction={}",graphics_scale.load(),dwg2::camera::percent.load(),dwg2::video::vsync.load(),dwg2::video::present_fps.load(),dwg2::video::fps_counter.load(),dwg2::video::turbo.load(),dwg2::prompts::choice.load(),dwg2::prompts::hud_correction.load());
}
}  // namespace dwg2::controls
#endif
