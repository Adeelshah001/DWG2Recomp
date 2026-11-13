#pragma once
#include <array>
#include <atomic>
#include <cstdint>

// Observation only. The 320 contiguous, 64-byte context slots cannot collide
// in this 512-slot table. Physical/virtual aliases select the same slot.
namespace dwg2::bgm_trace {
enum Counter { Init, Release, Enable, Disable, Refill0, Refill1, Valid0, Valid1,
               Consumer, Work, NoOutput, NoInput, OutputFull, Decode, Decoded,
               Count };
struct Slot { std::array<std::atomic<uint64_t>, Count> counts{}; };
inline std::array<Slot, 512> slots;
inline Slot& Get(uint32_t address) { return slots[((address & 0x1fffffffU) >> 6) % slots.size()]; }
inline void Hit(uint32_t address, Counter counter) {
  Get(address).counts[counter].fetch_add(1, std::memory_order_relaxed);
}
inline uint64_t Read(uint32_t address, Counter counter) {
  return Get(address).counts[counter].load(std::memory_order_relaxed);
}
}

namespace dwg2::bgm_trace {
inline std::atomic<uint64_t> callbacks_entered{}, callbacks_returned{}, submitted_frames{};
}
