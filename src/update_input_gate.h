#pragma once
#include <windows.h>
namespace dwg2::update_modal {
struct Event {
 HANDLE handle=nullptr;
 Event(){wchar_t name[256]{};DWORD size=GetEnvironmentVariableW(L"DWG2_UPDATE_MODAL_EVENT",name,256);if(size&&size<256)handle=OpenEventW(SYNCHRONIZE,FALSE,name);}
 ~Event(){if(handle)CloseHandle(handle);}
};
inline bool Active(){static Event event;return event.handle&&WaitForSingleObject(event.handle,0)==WAIT_OBJECT_0;}
inline bool GateUntilReleased(bool blocked,bool held,bool& waiting){if(blocked)waiting=true;if(!blocked&&!held)waiting=false;return blocked||waiting;}
}
