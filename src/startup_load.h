#pragma once
#include <cstdint>
namespace dwg2::startup {
struct LoadSnapshot {
 uint32_t caller=0,mode=0,state=0,buffer=0;
 uint32_t magic=0,count=0,stride=0,occupied=0,slot_type=0,data_offset=0,interim_offset=0;
};
inline bool CanLoadFirstSlot(const LoadSnapshot& s) {
 // This call site belongs to the title's startup load controller. Mode 0 is
 // SAVE, mode 1 is manual load, mode 2 is startup load. Never automate writes.
 if(s.caller!=0x82176B00||s.mode!=2||s.state!=5||!s.buffer)return false;
 if(s.magic!=0x00790001||s.count!=5||s.stride!=0x000A4000||!(s.occupied&1))return false;
 if(uint64_t(s.buffer)+0x00334000>0x100000000ULL)return false;
 if(s.data_offset<28||s.data_offset>=s.stride-16)return false;
 if(s.slot_type==13&&(s.interim_offset<28||s.interim_offset>=s.stride-16))return false;
 return true;
}
}
