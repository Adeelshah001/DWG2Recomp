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
    for(auto& entry:entries_) {
      if(entry.valid&&entry.address==address) {
        const bool changed=entry.percent!=percent;
        entry.percent=percent;
        return changed;
      }
    }
    entries_[next_]={address,percent,true};
    next_=(next_+1)%entries_.size();
    // Unknown/reused/evicted cameras must also restore the correct projection.
    return true;
  }
 private:
  struct Entry {uint32_t address=0;int percent=0;bool valid=false;};
  std::array<Entry,256> entries_{};
  uint32_t next_=0;
};
}
