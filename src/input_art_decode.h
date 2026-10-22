// CC0 glyph decoding. Source artwork and attribution are in assets/input-prompts.
#pragma once
#include <array>
#include <span>
#include <vector>
#include <stdexcept>
#include "input_art.h"
namespace dwg2::art {
inline std::vector<uint8_t> Decode(std::span<const uint8_t> packed,size_t expected) {
 std::vector<uint8_t> out;out.reserve(expected);
 for(size_t at=0;at<packed.size();) {
  unsigned control=packed[at++],n=control<128?control+1:control-127;
  if(out.size()+n>expected)throw std::runtime_error("Invalid input artwork length");
  if(control>=128) {
   if(at==packed.size())throw std::runtime_error("Truncated input artwork run");
   out.insert(out.end(),n,packed[at++]);
  } else {
   if(at+n>packed.size())throw std::runtime_error("Truncated input artwork pixels");
   out.insert(out.end(),packed.begin()+at,packed.begin()+at+n);at+=n;
  }
 }
 if(out.size()!=expected)throw std::runtime_error("Incomplete input artwork");
 return out;
}
inline const std::vector<uint8_t>& Glyph(unsigned index) {
 static const auto decoded=[] {
  std::array<std::vector<uint8_t>,12> values;
  for(size_t i=0;i<values.size();++i)values[i]=Decode({glyphs[i].data,glyphs[i].size},4096);
  return values;
 }();
 return decoded.at(index);
}
inline const std::vector<uint8_t>& Controller() {
 static const auto decoded=Decode(controller_bc3,131072);return decoded;
}
}
