#pragma once
#include <cstdint>
#include <cstring>
namespace dwg2::startup {
inline bool AdvanceStorageNotice(uint32_t caller,uint32_t mode,uint32_t state,const char*text){
 constexpr char notice[]="No storage device has been selected.";
 return caller==0x82176B00u&&mode==2&&state==1&&std::strncmp(text,notice,sizeof(notice)-1)==0;
}
}
