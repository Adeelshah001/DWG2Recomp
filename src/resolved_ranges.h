#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <tuple>
#include <vector>
namespace dwg2 {
// Track the latest owner of each 4 KiB resolved page. Unknown or partial writes
// cannot be migrated speculatively: the renderer must reject the transition.
class ResolvedRanges {
 public:
 struct Range { uint32_t address,length,format; };
 static constexpr uint32_t kPages=(512u*1024u*1024u)/4096u, kUnknown=UINT32_MAX;
 std::array<uint32_t,kPages> owners;
 std::vector<Range> records;
 std::map<std::tuple<uint32_t,uint32_t,uint32_t>,uint32_t> lookup;
 ResolvedRanges(){owners.fill(kUnknown);}
 void Clear(){owners.fill(kUnknown);records.clear();lookup.clear();}
 void Remember(uint32_t address,uint32_t length,uint32_t format) {
  if(!length || uint64_t(address)+length>uint64_t(kPages)*4096)return;
  uint32_t owner=kUnknown;
  if(!(address&4095) && !(length&4095) && records.size()<8192) {
   auto key=std::make_tuple(address,length,format);auto it=lookup.find(key);
   if(it!=lookup.end())owner=it->second;
   else {owner=static_cast<uint32_t>(records.size());records.push_back({address,length,format});lookup[key]=owner;}
  }
  for(uint32_t page=address/4096;page<=(address+length-1)/4096;++page)owners[page]=owner;
 }
 template<class IsValid> bool Plan(IsValid valid,std::vector<Range>& out) const {
  out.clear();
  for(uint32_t page=0;page<kPages;) {
   if(!valid(page)){++page;continue;}
   uint32_t owner=owners[page];if(owner==kUnknown)return false;
   const uint32_t first=page;++page;
   while(page<kPages && owners[page]==owner && valid(page))++page;
   out.push_back({first*4096,(page-first)*4096,records[owner].format});
  }
  return true;
 }
};
}
