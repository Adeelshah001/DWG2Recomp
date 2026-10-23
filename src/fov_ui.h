// Modeless host control; leaves the game window and its event loop running.
#pragma once
#include "camera_control.h"
#include <windows.h>
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <string>
namespace dwg2::camera {
inline HWND editor=nullptr;
inline bool SavePreference(const std::filesystem::path& path,int value) {
 if(!ValidPercent(value))return false;
 auto temporary=path;temporary+=L".tmp";
 {std::ofstream f(temporary);f<<"DWG2_FOV_V2 "<<value<<'\n';f.flush();if(!f)return false;}
 return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
struct EditorState {std::filesystem::path path;HWND slider=nullptr,label=nullptr;};
inline EditorState editor_state;
inline void UpdateLabel(EditorState& state,int value) {
 const auto label=value?L"+"+std::to_wstring(value)+L"%":std::wstring(L"Default");SetWindowTextW(state.label,label.c_str());
}
inline LRESULT CALLBACK EditorProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
 auto* state=reinterpret_cast<EditorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_CREATE) {
  state=static_cast<EditorState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
  auto label=CreateWindowW(L"STATIC",L"Field of view (updates while you play)",WS_CHILD|WS_VISIBLE,20,16,360,22,window,nullptr,nullptr,nullptr);
  state->label=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,165,46,80,24,window,nullptr,nullptr,nullptr);
  state->slider=CreateWindowW(TRACKBAR_CLASSW,L"Field of view",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_AUTOTICKS,20,76,360,42,window,reinterpret_cast<HMENU>(1),nullptr,nullptr);
  SendMessageW(state->slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,10));SendMessageW(state->slider,TBM_SETTICFREQ,1,0);SendMessageW(state->slider,TBM_SETPAGESIZE,0,1);SendMessageW(state->slider,TBM_SETPOS,TRUE,percent.load()/5);
  for(HWND child:{label,state->label,state->slider})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
  CreateWindowW(L"STATIC",L"Default",WS_CHILD|WS_VISIBLE,20,120,85,22,window,nullptr,nullptr,nullptr);CreateWindowW(L"STATIC",L"+50%",WS_CHILD|WS_VISIBLE|SS_RIGHT,300,120,80,22,window,nullptr,nullptr,nullptr);
  UpdateLabel(*state,percent.load());return 0;
 }
 if(message==WM_HSCROLL&&state&&reinterpret_cast<HWND>(lp)==state->slider) {
  const int value=int(SendMessageW(state->slider,TBM_GETPOS,0,0))*5;
  const int previous=percent.load();
  if(value!=previous) {
   if(SavePreference(state->path,value))percent.store(value);
   else {SendMessageW(state->slider,TBM_SETPOS,TRUE,previous/5);MessageBoxW(window,L"The FOV setting could not be saved. The previous value is retained.",L"Field of view",MB_ICONERROR);}
  }
  UpdateLabel(*state,percent.load());return 0;
 }
 if(message==WM_CLOSE){DestroyWindow(window);return 0;}
 if(message==WM_NCDESTROY){if(editor==window)editor=nullptr;SetWindowLongPtrW(window,GWLP_USERDATA,0);}
 return DefWindowProcW(window,message,wp,lp);
}
inline void ShowEditor(HWND owner,const std::filesystem::path& path) {
 if(editor&&IsWindow(editor)){ShowWindow(editor,SW_SHOWNORMAL);SetForegroundWindow(editor);return;}
 INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
 WNDCLASSW cls{};cls.lpfnWndProc=EditorProcedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"DWG2.FovSlider";cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&cls);
 RECT parent{};GetWindowRect(owner,&parent);editor_state=EditorState{path};auto* state=&editor_state;
 editor=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Field of View",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,parent.left+40,parent.top+80,416,190,owner,nullptr,cls.hInstance,state);
 // One modeless editor uses fixed host state; failed creation cannot leak.
 if(editor){ShowWindow(editor,SW_SHOW);SetFocus(state->slider);}
}
}
