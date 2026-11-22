#pragma once
#include <array>
#include <cstdint>

namespace dwg2::camera {
// Each camera owns a cached projection and view-projection matrix. Refresh each
// one after a preference change, including restoring Default. No guest pointers
// are retained and the fixed host table never allocates in the frame loop.
class RefreshTracker {
 public:
  bool NeedsRefresh(uint32_t address,int percent) {
    if(last_valid_&&last_address_==address&&last_percent_==percent)return false;
    for(auto& entry:entries_) {
      if(entry.valid&&entry.address==address) {
        const bool changed=entry.percent!=percent;
        entry.percent=percent;
        last_address_=address;last_percent_=percent;last_valid_=true;
        return changed;
      }
    }
    entries_[next_]={address,percent,true};
    next_=(next_+1)%entries_.size();
    last_address_=address;last_percent_=percent;last_valid_=true;
    // Unknown/reused/evicted cameras must also restore the correct projection.
    return true;
  }
 private:
  struct Entry {uint32_t address=0;int percent=0;bool valid=false;};
  std::array<Entry,256> entries_{};
  uint32_t next_=0;
  uint32_t last_address_=0;
  int last_percent_=0;
  bool last_valid_=false;
};
}
