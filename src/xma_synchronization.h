// Original host synchronization for the shared guest XMA context records.
#pragma once
#include <array>
#include <cstdint>
#include <mutex>

namespace dwg2::xma {
// Contexts are 64-byte aligned. Aliases of physical guest memory must select
// the same stripe. Collisions only serialize independent contexts.
inline std::array<std::recursive_mutex, 256> context_mutexes;
inline std::recursive_mutex& Mutex(uint32_t address) {
  return context_mutexes[((address & 0x1fffffffU) >> 6) % context_mutexes.size()];
}
using Guard = std::lock_guard<std::recursive_mutex>;

template<class Context>
inline void MergeDecoder(Context& fresh, const Context& decoded, const Context& initial) {
  fresh.loop_count = decoded.loop_count;
  fresh.output_buffer_write_offset = decoded.output_buffer_write_offset;
  if (initial.input_buffer_0_valid && !decoded.input_buffer_0_valid)
    fresh.input_buffer_0_valid = 0;
  if (initial.input_buffer_1_valid && !decoded.input_buffer_1_valid)
    fresh.input_buffer_1_valid = 0;
  if (initial.output_buffer_valid && !decoded.output_buffer_valid)
    fresh.output_buffer_valid = 0;
  fresh.input_buffer_read_offset = decoded.input_buffer_read_offset;
  fresh.error_status = decoded.error_status;
  fresh.current_buffer = decoded.current_buffer;
  // The output consumer belongs to the guest. Restoring the decoder snapshot
  // here rewinds consumption and can eventually strand a streaming voice.
}
}
