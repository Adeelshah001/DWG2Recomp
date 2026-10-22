// Original DWG2 host UI. No game code or assets.
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <filesystem>
#include <rex/ui/window_sdl.h>
#include "input_ui.h"
#include "resolution_control.h"
#include "camera_control.h"

namespace dwg2::controls {
constexpr UINT kPause = 41001, kQuit = 41002, kFullscreen = 41003;
constexpr UINT kRestart = 41004;
constexpr UINT kFovDefault=41060,kFovExtended=41061;
constexpr UINT kScale1 = 41011, kScale3 = 41013, kHelp = 41020;
constexpr UINT kConfigure = 41030;
constexpr UINT kPromptAuto=41040,kPromptXbox=41041,kPromptPlayStation=41042,kHudCorrection=41050;
constexpr UINT_PTR kReleaseStart = 0x44574732;
constexpr UINT_PTR kResolutionTimer = 0x44574733;
struct State {
  rex::ui::WindowSDL* owner;
  SDL_Window* sdl;
  HMENU menu;
  bool start_down = false;
  int pending_scale = 0;
};
inline void StartKey(State* state, bool down) {
  dwg2::input::pause_pressed.store(down);
  state->start_down = down;
}
inline std::filesystem::path SettingsPath() {
  wchar_t exe[32768]{};
  GetModuleFileNameW(nullptr, exe, 32768);
  return std::filesystem::path(exe).parent_path().parent_path() / L"display.ini";
}
inline void Sync(rex::ui::WindowSDL* owner) {
  HWND hwnd = static_cast<HWND>(owner->GetNativeWindowHandle());
  auto* s = reinterpret_cast<State*>(GetPropW(hwnd, L"DWG2.Controls"));
  if (!s) return;
  HMENU desired = owner->IsFullscreen() ? nullptr : s->menu;
  if (GetMenu(hwnd) != desired) { SetMenu(hwnd, desired); DrawMenuBar(hwnd); }
  CheckMenuItem(s->menu, kFullscreen, MF_BYCOMMAND |
                (owner->IsFullscreen() ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuRadioItem(s->menu,kPromptAuto,kPromptPlayStation,kPromptAuto+dwg2::prompts::choice.load(),MF_BYCOMMAND);
  CheckMenuItem(s->menu,kHudCorrection,MF_BYCOMMAND|(dwg2::prompts::hud_correction.load()?MF_CHECKED:MF_UNCHECKED));
  CheckMenuRadioItem(s->menu,kFovDefault,kFovExtended,kFovDefault+int(dwg2::camera::extended.load()),MF_BYCOMMAND);
}
inline bool SavePrompts(int choice,bool hud) {
 auto path=dwg2::input::Root()/L"prompts.ini";auto temporary=path;temporary+=L".tmp";
 {std::ofstream f(temporary);f<<choice<<' '<<int(hud)<<'\n';f.flush();if(!f)return false;}
 return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
inline LRESULT CALLBACK Procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                   UINT_PTR id, DWORD_PTR data) {
  auto* s = reinterpret_cast<State*>(data);
  if (msg == WM_APP + 0x212) {
    s->owner->SetFullscreen(!s->owner->IsFullscreen()); return 0;
  }
  if (msg == WM_COMMAND && HIWORD(wp) == 0) {
    UINT command = LOWORD(wp);
    if(command==kFovDefault||command==kFovExtended) {
      auto path=dwg2::input::Root()/L"camera.ini";auto temporary=path;temporary+=L".tmp";
      const int extended=command==kFovExtended?1:0;bool written=false;
      {std::ofstream file(temporary);file<<extended<<'\n';file.flush();written=bool(file);}
      if(written&&MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        dwg2::camera::extended.store(extended!=0);Sync(s->owner);
      } else MessageBoxW(hwnd,L"The camera setting could not be saved. Your previous setting is retained.",L"Field of view",MB_ICONERROR);
      return 0;
    }
    if((command>=kPromptAuto&&command<=kPromptPlayStation)||command==kHudCorrection) {
      int choice=command==kHudCorrection?dwg2::prompts::choice.load():int(command-kPromptAuto);
      bool hud=dwg2::prompts::hud_correction.load();if(command==kHudCorrection)hud=!hud;
      if(!SavePrompts(choice,hud))MessageBoxW(hwnd,L"Could not save prompt/display settings. Your previous settings are retained.",L"DWG2",MB_ICONERROR);
      else {dwg2::prompts::choice.store(choice);dwg2::prompts::hud_correction.store(hud);Sync(s->owner);}
      return 0;
    }
    if (command == kConfigure) { dwg2::input::ShowEditor(hwnd); return 0; }
    if (command == kFullscreen) {
      s->owner->SetFullscreen(!s->owner->IsFullscreen()); return 0;
    }
    if (command == kPause) {
      if (!s->start_down) {
        StartKey(s, true);
        SetTimer(hwnd, kReleaseStart, 150, nullptr);
      }
      return 0;
    }
    if (command == kQuit) { PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0; }
    if (command == kRestart) {
      wchar_t name[128]{};
      DWORD length=GetEnvironmentVariableW(L"DWG2_RESTART_EVENT",name,128);
      HANDLE event=(length&&length<128)?OpenEventW(EVENT_MODIFY_STATE,FALSE,name):nullptr;
      if(!event) {
        MessageBoxW(hwnd,L"Start the game with START_DWG2.exe to use Restart.",L"Restart",MB_ICONINFORMATION);
        return 0;
      }
      BOOL requested=SetEvent(event);
      if(requested&&!PostMessageW(hwnd,WM_CLOSE,0,0)){ResetEvent(event);requested=FALSE;}
      CloseHandle(event);
      if(!requested)MessageBoxW(hwnd,L"Restart could not be requested. Your game is still running.",L"Restart",MB_ICONERROR);
      return 0;
    }
    if (command >= kScale1 && command <= kScale3) {
      int scale = int(command - kScale1 + 1);
      s->pending_scale=scale;
      dwg2::resolution::completed.store(0);
      dwg2::resolution::error.store(0);
      dwg2::resolution::requested.store(scale);
      SetTimer(hwnd, kResolutionTimer, 50, nullptr);
      return 0;
    }
    if (command == kHelp) {
      MessageBoxW(hwnd,
        L"Dynasty Warriors: Gundam 2 Recomp - local v02 preview\n\n"
        L"Controls > Configure: keyboard, PS5 and Xbox remapping.\n"
        L"Controls > Button Prompts: Auto, Xbox or PlayStation icons.\n"
        L"View > HUD seam correction: toggle atlas-only sampling correction.\n"
        L"Fullscreen: PS5 touchpad click, Alt+Enter, or View > Fullscreen.\n"
        L"File > Pause / Resume sends Start regardless of your bindings.\n"
        L"File > Restart starts a fresh game process through the launcher.\n"
        L"View > Field of View: Default or a 10% wider perspective angle.\n"
        L"View > Resolution changes internal rendering and output live.\n"
        L"1x: 1280 x 720; 2x: 2560 x 1440; 3x: 3840 x 2160.\n"
        L"A resolution change may briefly rebuild GPU resources.\n"
        L"Save from the game's menu before restarting or quitting.\n"
        L"Keyboard and controller work together. Startup movies are skipped.",
        L"Help", MB_OK | MB_ICONINFORMATION);
      return 0;
    }
  }
  if(msg == WM_TIMER && wp == kResolutionTimer) {
      const int scale=dwg2::resolution::completed.exchange(0);
      const int error=dwg2::resolution::error.exchange(0);
      if(error && error==s->pending_scale) {
        KillTimer(hwnd,kResolutionTimer);
        MessageBoxW(hwnd,L"The renderer could not apply this resolution. Your previous resolution is still active.",L"Resolution",MB_ICONERROR);
        return 0;
      }
      if(!scale || scale!=s->pending_scale)return 0;
      KillTimer(hwnd,kResolutionTimer);
      s->pending_scale=0;
      auto file = SettingsPath();
      auto temporary = file; temporary += L".tmp";
      { std::ofstream out(temporary); out << scale << '\n'; }
      if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        MessageBoxW(hwnd, L"Could not save the display setting. Check that the game folder is writable.", L"DWG2", MB_OK | MB_ICONERROR);
        return 0;
      }
      if(!s->owner->IsFullscreen()) {
        SDL_RestoreWindow(s->sdl);
        SDL_SetWindowSize(s->sdl,1280*scale,720*scale);
      }
      CheckMenuRadioItem(s->menu,kScale1,kScale3,kScale1+scale-1,MF_BYCOMMAND);
      return 0;
  }
  if (msg == WM_TIMER && wp == kReleaseStart) {
    KillTimer(hwnd, kReleaseStart); StartKey(s, false); return 0;
  }
  if (msg == WM_SYSKEYDOWN && wp == VK_RETURN && !(lp & (1LL << 30))) {
    s->owner->SetFullscreen(!s->owner->IsFullscreen()); return 0;
  }
  if (msg == WM_NCDESTROY) {
    KillTimer(hwnd, kReleaseStart);
    KillTimer(hwnd, kResolutionTimer);
    dwg2::input::pause_pressed.store(false);
    if(dwg2::input::editor_window)DestroyWindow(dwg2::input::editor_window);
    RemovePropW(hwnd, L"DWG2.Controls");
    RemoveWindowSubclass(hwnd, Procedure, id);
    SetMenu(hwnd, nullptr);
    DestroyMenu(s->menu);
    delete s;
  }
  return DefSubclassProc(hwnd, msg, wp, lp);
}
inline void Install(rex::ui::WindowSDL* owner, SDL_Window* sdl) {
  HWND hwnd = static_cast<HWND>(owner->GetNativeWindowHandle());
  if (!hwnd || GetPropW(hwnd, L"DWG2.Controls")) return;
  HMENU menu = CreateMenu(), file = CreatePopupMenu(), view = CreatePopupMenu();
  HMENU sizes = CreatePopupMenu(), help = CreatePopupMenu(), controls = CreatePopupMenu(), prompts=CreatePopupMenu(), fov=CreatePopupMenu();
  AppendMenuW(file, MF_STRING, kPause, L"&Pause / Resume");
  AppendMenuW(file, MF_STRING, kRestart, L"&Restart");
  AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(file, MF_STRING, kQuit, L"&Quit Game");
  AppendMenuW(view, MF_STRING, kFullscreen, L"&Fullscreen\tAlt+Enter");
  AppendMenuW(sizes, MF_STRING, kScale1, L"&1x - 1280 x 720");
  AppendMenuW(sizes, MF_STRING, kScale1+1, L"&2x - 2560 x 1440");
  AppendMenuW(sizes, MF_STRING, kScale3, L"&3x - 3840 x 2160");
  AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(sizes), L"&Resolution");
  AppendMenuW(view,MF_STRING,kHudCorrection,L"HUD seam correction");
  AppendMenuW(fov,MF_STRING,kFovDefault,L"&Default");
  AppendMenuW(fov,MF_STRING,kFovExtended,L"&Extended (+10%)");
  AppendMenuW(view,MF_POPUP,reinterpret_cast<UINT_PTR>(fov),L"Field of &View");
  AppendMenuW(controls,MF_STRING,kConfigure,L"&Configure Controller / Keyboard...");
  AppendMenuW(prompts,MF_STRING,kPromptAuto,L"&Auto");
  AppendMenuW(prompts,MF_STRING,kPromptXbox,L"&Xbox");
  AppendMenuW(prompts,MF_STRING,kPromptPlayStation,L"&PlayStation");
  AppendMenuW(controls,MF_POPUP,reinterpret_cast<UINT_PTR>(prompts),L"Button &Prompts");
  AppendMenuW(help, MF_STRING, kHelp, L"&Controls and About");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&View");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(controls), L"&Controls");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");
  auto* s = new State{owner, sdl, menu};
  if (!SetPropW(hwnd, L"DWG2.Controls", s) || !SetWindowSubclass(hwnd, Procedure, 0x44574732, reinterpret_cast<DWORD_PTR>(s))) {
    RemovePropW(hwnd, L"DWG2.Controls"); DestroyMenu(menu); delete s; return;
  }
  RECT client{};
  GetClientRect(hwnd, &client);
  {std::ifstream in(dwg2::input::Root()/L"camera.ini");int extended=0;
   if(in>>extended&&(extended==0||extended==1))dwg2::camera::extended.store(extended!=0);}
  {std::ifstream in(dwg2::input::Root()/L"prompts.ini");int choice=0,hud=1;
   if(in>>choice>>hud&&choice>=0&&choice<=2&&(hud==0||hud==1)) {
    dwg2::prompts::choice.store(choice);dwg2::prompts::hud_correction.store(hud!=0);
   }}
  Sync(owner);
  { int scale=1;std::ifstream in(SettingsPath());in>>scale;
    if(scale>=1 && scale<=3)CheckMenuRadioItem(menu,kScale1,kScale3,kScale1+scale-1,MF_BYCOMMAND); }
  if(!dwg2::input::Load())MessageBoxW(hwnd,L"controls.ini could not be read. Default mappings are active; the file is preserved until you save new mappings.",L"Controls",MB_ICONWARNING);
  if (!owner->IsFullscreen()) {
    // Attaching a native menu consumes client height unless the frame grows.
    RECT frame{0, 0, client.right, client.bottom};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)),
                            TRUE, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)),
                            GetDpiForWindow(hwnd));
    SetWindowPos(hwnd, nullptr, 0, 0, frame.right-frame.left, frame.bottom-frame.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}
}  // namespace dwg2::controls
#endif
