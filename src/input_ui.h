// Original native mapping editor, inspired by emulator pad configuration workflows.
#pragma once
#include "input_host.h"
#include <commctrl.h>
#include <vector>
namespace dwg2::input {
inline HWND editor_window=nullptr;
struct Editor {
 HWND owner{},window{},profile{},device{},list{},status{},deadzone{};
 std::array<Profile,3> working{};
 std::vector<SDL_Gamepad*> pads;
 int type=0,capturing=-1,ticks=0;bool armed=false;
 int modifier_candidate=0,modifier_bits=0;
 std::array<bool,256> blocked_keys{};
 std::vector<Raw> blocked_pads;
};
inline std::wstring Label(Binding b,int type) {
 if(!b.kind)return L"Unbound";
 std::wstring text;
 if(b.kind==1) {
  if(b.mods&1)text+=L"Shift + ";if(b.mods&2)text+=L"Ctrl + ";if(b.mods&4)text+=L"Alt + ";
  wchar_t key[100]{};UINT scan=MapVirtualKeyW(b.code,MAPVK_VK_TO_VSC);
  if(b.code>=33 && b.code<=46)scan|=0x100;
  GetKeyNameTextW(scan<<16,key,100);text+=key[0]?key:L"Key "+std::to_wstring(b.code);
 } else if(b.kind==2) {
  constexpr const wchar_t* xbox[]={L"A",L"B",L"X",L"Y",L"View",L"Guide",L"Menu",L"Left stick click",L"Right stick click",L"LB",L"RB",L"D-pad Up",L"D-pad Down",L"D-pad Left",L"D-pad Right",L"Misc 1",L"Paddle 1",L"Paddle 2",L"Paddle 3",L"Paddle 4",L"Touchpad",L"Misc 2",L"Misc 3",L"Misc 4",L"Misc 5",L"Misc 6"};
  constexpr const wchar_t* ps[]={L"Cross",L"Circle",L"Square",L"Triangle",L"Create",L"PS",L"Options",L"L3",L"R3",L"L1",L"R1"};
  text=type==1 && b.code<11?ps[b.code]:xbox[b.code];
 } else {
  constexpr const wchar_t* axes[]={L"Left stick X",L"Left stick Y",L"Right stick X",L"Right stick Y",L"LT / L2",L"RT / R2"};
  text=axes[b.code];if(b.code<4)text+=b.sign<0?L" -":L" +";
 }
 return text;
}
inline void RefreshRows(Editor* e) {
 for(int i=0;i<Count;++i) {
  auto text=Label(e->working[e->type].bind[i],e->type);
  const auto alt=e->working[e->type].alternate[i];if(alt.kind)text+=L"  /  "+Label(alt,e->type);
  LVITEMW item{};item.iSubItem=1;item.pszText=text.data();SendMessageW(e->list,LVM_SETITEMTEXTW,i,reinterpret_cast<LPARAM>(&item));
 }
 SendMessageW(e->deadzone,TBM_SETPOS,TRUE,e->working[e->type].deadzone);
}
inline void RefreshPads(Editor* e) {
 for(auto* pad:e->pads)SDL_CloseGamepad(pad);e->pads.clear();
 SendMessageW(e->device,CB_RESETCONTENT,0,0);
 SendMessageW(e->device,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(e->type==0?L"Keyboard":L"Any connected device of this type"));
 if(e->type) {
  int count=0;auto* ids=SDL_GetGamepads(&count);
  for(int i=0;i<count;++i) {
   auto* pad=SDL_OpenGamepad(ids[i]);if(!pad)continue;
   if(Family(pad)!=e->type){SDL_CloseGamepad(pad);continue;}
   e->pads.push_back(pad);const auto* utf8=SDL_GetGamepadName(pad);
   wchar_t name[256]{};MultiByteToWideChar(CP_UTF8,0,utf8?utf8:"Controller",-1,name,256);
   SendMessageW(e->device,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
  }
  SDL_free(ids);
 }
 SendMessageW(e->device,CB_SETCURSEL,0,0);e->capturing=-1;
}
inline void BeginCapture(Editor* e) {
 const int selected=ListView_GetNextItem(e->list,-1,LVNI_SELECTED);
 if(selected<0){SetWindowTextW(e->status,L"Select a game control first.");return;}
 e->capturing=selected;e->armed=false;e->modifier_candidate=0;e->blocked_pads.clear();
 for(int k=0;k<256;++k)e->blocked_keys[k]=(GetAsyncKeyState(k)&0x8000)!=0;
 for(auto* pad:e->pads)e->blocked_pads.push_back(ReadPad(pad));
 SetWindowTextW(e->status,e->type==0?L"Press a key or key combination. Esc cancels.":L"Press a controller button, trigger or stick direction. Esc cancels.");
}
inline void Capture(Editor* e) {
 if(e->capturing<0)return;
 if(GetForegroundWindow()!=e->window)return;
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000){e->capturing=-1;SetWindowTextW(e->status,L"Binding cancelled.");return;}
 Binding binding;
 if(!e->type) {
  for(int k=8;k<255;++k) {
   const bool down=(GetAsyncKeyState(k)&0x8000)!=0;
   if(!down)e->blocked_keys[k]=false;
   if(!down || e->blocked_keys[k] || k==16 || k==17 || k==18 || (k>=160 && k<=165) || k==27)continue;
   binding={1,k,1,((GetAsyncKeyState(VK_SHIFT)&0x8000)?1:0)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?4:0)};break;
  }
  if(!binding.kind)for(int key:{VK_SHIFT,VK_CONTROL,VK_MENU}) {
   const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
   if(down && !e->blocked_keys[key]) {
    e->modifier_candidate=key;
    const int bit=key==VK_SHIFT?1:(key==VK_CONTROL?2:4);
    e->modifier_bits=(((GetAsyncKeyState(VK_SHIFT)&0x8000)?1:0)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?4:0))&~bit;
   } else if(!down && e->modifier_candidate==key) {binding={1,key,1,e->modifier_bits};break;}
  }
 } else {
  int device=static_cast<int>(SendMessageW(e->device,CB_GETCURSEL,0,0));
  for(size_t p=0;p<e->pads.size() && !binding.kind;++p) {
   if(device>0 && p!=static_cast<size_t>(device-1))continue;
   if(!SDL_GamepadConnected(e->pads[p]))continue;
   auto raw=ReadPad(e->pads[p]);auto& blocked=e->blocked_pads[p];
   for(int b=0;b<26;++b) {
    if(!raw.buttons[b])blocked.buttons[b]=false;
    if(raw.buttons[b] && !blocked.buttons[b]){binding={2,b};break;}
   }
   for(int a=0;a<6 && !binding.kind;++a) {
    if(std::abs(raw.axes[a])<.25f)blocked.axes[a]=0;
    if(std::abs(raw.axes[a])>.65f && std::abs(blocked.axes[a])<.25f)binding={3,a,raw.axes[a]<0?-1:1};
   }
  }
 }
 if(binding.kind) {
  e->working[e->type].bind[e->capturing]=binding;e->working[e->type].alternate[e->capturing]={};
  e->capturing=-1;RefreshRows(e);SetWindowTextW(e->status,L"Binding changed. Apply to save it and use it immediately.");
 }
}
inline bool Apply(Editor* e) {
 e->working[e->type].deadzone=static_cast<int>(SendMessageW(e->deadzone,TBM_GETPOS,0,0));
 if(!Save(e->working)){MessageBoxW(e->window,L"Could not save controls.ini. Your active mappings were not changed.",L"Controls",MB_ICONERROR);return false;}
 SetWindowTextW(e->status,L"Mappings saved and applied. They will be reused next launch.");return true;
}
inline LRESULT CALLBACK EditorProc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
 auto* e=reinterpret_cast<Editor*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(msg==WM_NCCREATE) {
  e=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
  e->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(e));
 }
 if(!e)return DefWindowProcW(window,msg,wp,lp);
 if(msg==WM_COMMAND) {
  int id=LOWORD(wp);
  if(id==100 && HIWORD(wp)==CBN_SELCHANGE) {
   e->working[e->type].deadzone=static_cast<int>(SendMessageW(e->deadzone,TBM_GETPOS,0,0));
   e->type=static_cast<int>(SendMessageW(e->profile,CB_GETCURSEL,0,0));RefreshPads(e);RefreshRows(e);
  }
  if(id==101 && HIWORD(wp)==CBN_SELCHANGE)e->capturing=-1;
  if(id==110)BeginCapture(e);
  if(id==111) {int row=ListView_GetNextItem(e->list,-1,LVNI_SELECTED);if(row>=0){e->working[e->type].bind[row]={};e->working[e->type].alternate[row]={};RefreshRows(e);}}
  if(id==112){e->working[e->type]=Defaults(e->type);RefreshRows(e);SetWindowTextW(e->status,L"Defaults restored for this profile. Apply to save.");}
  if(id==113)RefreshPads(e);
  if(id==114)Apply(e);
  if(id==IDOK && Apply(e))DestroyWindow(window);
  if(id==IDCANCEL)DestroyWindow(window);
  return 0;
 }
 if(msg==WM_NOTIFY && reinterpret_cast<NMHDR*>(lp)->hwndFrom==e->list && reinterpret_cast<NMHDR*>(lp)->code==NM_DBLCLK){BeginCapture(e);return 0;}
 if(msg==WM_TIMER){SDL_PumpEvents();Capture(e);return 0;}
 if(msg==WM_CLOSE){DestroyWindow(window);return 0;}
 if(msg==WM_NCDESTROY) {
  KillTimer(window,1);for(auto* pad:e->pads)SDL_CloseGamepad(pad);
  editing.store(false);editor_window=nullptr;EnableWindow(e->owner,TRUE);SetForegroundWindow(e->owner);
  SetWindowLongPtrW(window,GWLP_USERDATA,0);delete e;
 }
 return DefWindowProcW(window,msg,wp,lp);
}
inline void ShowEditor(HWND owner) {
 if(editor_window){SetForegroundWindow(editor_window);return;}
 INITCOMMONCONTROLSEX init{sizeof(init),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&init);
 WNDCLASSW klass{};klass.lpfnWndProc=EditorProc;klass.hInstance=GetModuleHandleW(nullptr);
 klass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));klass.hbrBackground=GetSysColorBrush(COLOR_3DFACE);klass.lpszClassName=L"DWG2.MappingEditor";
 RegisterClassW(&klass);
 auto* e=new Editor;e->owner=owner;{std::lock_guard lock(mutex);e->working=profiles;}
 const DWORD style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;RECT rect{0,0,840,710};AdjustWindowRectEx(&rect,style,FALSE,WS_EX_DLGMODALFRAME);
 HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,klass.lpszClassName,L"DWG2 - Controller and Keyboard Configuration",style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,owner,nullptr,klass.hInstance,e);
 if(!window){delete e;return;}editor_window=window;
 auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD flags,int x,int y,int width,int height,int id)->HWND {
  HWND item=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|flags,x,y,width,height,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),klass.hInstance,nullptr);
  SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return item;
 };
 add(L"STATIC",L"Input profile",0,16,14,120,20,0);
 e->profile=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,16,36,250,200,100);
 for(const auto* name:{L"Keyboard",L"PS5 / DualSense",L"Xbox controller"})SendMessageW(e->profile,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
 SendMessageW(e->profile,CB_SETCURSEL,0,0);
 add(L"STATIC",L"Capture input from",0,284,14,250,20,0);
 e->device=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,284,36,400,200,101);
 add(L"BUTTON",L"Refresh devices",WS_TABSTOP,696,35,128,26,113);
 e->list=add(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER|WS_TABSTOP,16,80,808,490,102);
 ListView_SetExtendedListViewStyle(e->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
 LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=280;column.pszText=const_cast<wchar_t*>(L"Game control");SendMessageW(e->list,LVM_INSERTCOLUMNW,0,reinterpret_cast<LPARAM>(&column));
 column.cx=500;column.pszText=const_cast<wchar_t*>(L"Assigned input (double-click to bind)");SendMessageW(e->list,LVM_INSERTCOLUMNW,1,reinterpret_cast<LPARAM>(&column));
 for(int i=0;i<Count;++i){LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=i;item.pszText=const_cast<wchar_t*>(names[i]);SendMessageW(e->list,LVM_INSERTITEMW,0,reinterpret_cast<LPARAM>(&item));}
 ListView_SetItemState(e->list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
 add(L"BUTTON",L"Bind selected",WS_TABSTOP,16,582,120,28,110);add(L"BUTTON",L"Clear binding",WS_TABSTOP,146,582,120,28,111);
 add(L"BUTTON",L"Restore defaults",WS_TABSTOP,276,582,130,28,112);
 add(L"STATIC",L"Stick / trigger deadzone (0-50%)",0,432,580,300,20,0);
 e->deadzone=add(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,432,600,380,30,103);
 SendMessageW(e->deadzone,TBM_SETRANGE,TRUE,MAKELPARAM(0,50));SendMessageW(e->deadzone,TBM_SETTICFREQ,10,0);
 e->status=add(L"STATIC",L"Select an input profile, then double-click a control to bind it. Alt+Enter remains a host shortcut.",0,16,638,808,24,104);
 add(L"BUTTON",L"Apply",WS_TABSTOP,474,674,100,28,114);add(L"BUTTON",L"Save and close",BS_DEFPUSHBUTTON|WS_TABSTOP,584,674,130,28,IDOK);add(L"BUTTON",L"Cancel",WS_TABSTOP,724,674,100,28,IDCANCEL);
 RefreshRows(e);RefreshPads(e);editing.store(true);EnableWindow(owner,FALSE);
 SetTimer(window,1,35,nullptr);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
}
}
