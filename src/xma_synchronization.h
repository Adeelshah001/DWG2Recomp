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

inline constexpr uint32_t kBitsPerPacket = 2048 * 8;
inline constexpr uint32_t kBitsPerPacketHeader = 32;

constexpr bool HasFrameStart(uint32_t packet_frame_offset) {
  return packet_frame_offset >= kBitsPerPacketHeader && packet_frame_offset < kBitsPerPacket;
}

inline uint32_t NextPacketIndex(uint32_t packet_index, uint8_t skip_count, uint32_t packet_count) {
  return skip_count == 0xFF ? packet_count : packet_index + uint32_t(skip_count) + 1;
}

inline uint32_t FindNextPacketReadOffset(const uint8_t* buffer, uint32_t start_packet_index,
                                         uint32_t packet_count) {
  if (!buffer) return kBitsPerPacketHeader;
  for (uint32_t idx = start_packet_index; idx < packet_count;) {
    const uint8_t* pkt = buffer + std::size_t(idx) * 2048u;
    const uint32_t val =
        (uint32_t(pkt[0] & 0x3u) << 13) | (uint32_t(pkt[1]) << 5) | (uint32_t(pkt[2]) >> 3);
    const uint32_t offset = val + kBitsPerPacketHeader;
    if (HasFrameStart(offset)) return idx * kBitsPerPacket + offset;
    const uint8_t skip = pkt[3];
    if (skip == 0xFF) return kBitsPerPacketHeader;
    idx += uint32_t(skip) + 1u;
  }
  return kBitsPerPacketHeader;
}
}
