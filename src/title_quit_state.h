#pragma once
#include <cstdint>
namespace dwg2::title {
constexpr uint32_t kAccept=0x1000,kDecline=0x2000;
enum class Action { Forward, Consume, Open, Decline, Accept };
struct QuitState {
 uint32_t owner=0,controller=0;bool visible=false,armed=false,closing=false,selectedYes=true;
 Action Update(uint32_t object,bool ready,bool focused,uint32_t pressed0,uint32_t held0,uint32_t pressed1,uint32_t held1) {
  if(!ready||owner!=object){*this={};if(!ready)return Action::Forward;owner=object;}
  if(!focused){armed=false;return visible?Action::Consume:Action::Forward;}
  if(closing)return Action::Consume;
  if(!visible){
   if(!(pressed0&kDecline)&&!(pressed1&kDecline))return Action::Forward;
   controller=(pressed0&kDecline)?0:1;selectedYes=true;visible=true;armed=false;return Action::Open;
  }
  const uint32_t pressed=controller?pressed1:pressed0,held=controller?held1:held0;
  if(!armed){if(!((held|pressed)&(kAccept|kDecline)))armed=true;return Action::Consume;}
  // Decline wins simultaneous A+B. Start and unrelated buttons never accept.
  if(pressed&kDecline){visible=false;armed=false;return Action::Decline;}
  if(pressed&0x4)selectedYes=true;
  if(pressed&0x8)selectedYes=false;
  if(pressed&kAccept){if(!selectedYes){visible=false;armed=false;return Action::Decline;}closing=true;return Action::Accept;}
  return Action::Consume;
 }
 void CloseFailed(){closing=false;armed=false;}
};
}
