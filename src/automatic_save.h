#pragma once
#include <cstdint>
namespace dwg2::saving {
struct Snapshot {
 uint32_t caller=0,mode=0,state=0,ready=0,buffer=0;
 uint32_t magic=0,count=0,stride=0,occupied=0,data_offset=0;
};
inline bool CanSaveFirstSlot(const Snapshot& s) {
 // Dedicated save-screen update entry points; the Options save/load controller
 // (82168934/82168984) and startup/manual load callers remain untouched.
 if(s.caller!=0x8216D360&&s.caller!=0x8227E76C)return false;
 if(s.mode!=0||s.state!=5||s.ready!=2||!s.buffer)return false;
 if(uint64_t(s.buffer)+0x00334000>0x100000000ULL)return false;
 if(s.magic!=0x00790001||s.count!=5||s.stride!=0x000A4000)return false;
 // A valid existing container is required. First-time/damaged-save recovery
 // remains in the game's original UI rather than guessing how to initialize it.
 if(s.data_offset<28||s.data_offset>=s.stride-16)return false;
 return true;
}
}
