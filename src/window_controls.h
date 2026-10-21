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

namespace dwg2::controls {
constexpr UINT kPause = 41001, kQuit = 41002, kFullscreen = 41003;
constexpr UINT kScale1 = 41011, kScale3 = 41013, kHelp = 41020;
constexpr UINT kConfigure = 41030;
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
}
inline LRESULT CALLBACK Procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                   UINT_PTR id, DWORD_PTR data) {
  auto* s = reinterpret_cast<State*>(data);
  if (msg == WM_APP + 0x212) {
    s->owner->SetFullscreen(!s->owner->IsFullscreen()); return 0;
  }
  if (msg == WM_COMMAND && HIWORD(wp) == 0) {
    UINT command = LOWORD(wp);
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
        L"Fullscreen: PS5 touchpad click, Alt+Enter, or View > Fullscreen.\n"
        L"File > Pause / Resume sends Start regardless of your bindings.\n"
        L"View > Resolution changes internal rendering and output live.\n"
        L"1x: 1280 x 720; 2x: 2560 x 1440; 3x: 3840 x 2160.\n"
        L"A resolution change may briefly rebuild GPU resources.\n"
        L"Save from the game's menu before quitting.\n"
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
  HMENU sizes = CreatePopupMenu(), help = CreatePopupMenu(), controls = CreatePopupMenu();
  AppendMenuW(file, MF_STRING, kPause, L"&Pause / Resume");
  AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(file, MF_STRING, kQuit, L"&Quit Game");
  AppendMenuW(view, MF_STRING, kFullscreen, L"&Fullscreen\tAlt+Enter");
  AppendMenuW(sizes, MF_STRING, kScale1, L"&1x - 1280 x 720");
  AppendMenuW(sizes, MF_STRING, kScale1+1, L"&2x - 2560 x 1440");
  AppendMenuW(sizes, MF_STRING, kScale3, L"&3x - 3840 x 2160");
  AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(sizes), L"&Resolution");
  AppendMenuW(controls,MF_STRING,kConfigure,L"&Configure Controller / Keyboard...");
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
