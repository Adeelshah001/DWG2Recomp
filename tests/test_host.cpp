#include "automatic_save.h"
#include "camera_control.h"
#include "camera_refresh.h"
#include "keyboard_art.h"
#include "prompt_control.h"
#include "runtime_pause.h"
#include "startup_load.h"
#include "video_control.h"
#include "xma_synchronization.h"
#include <array>
#include <cstdio>

extern "C" std::atomic<uint32_t> Dwg2PauseRequested{0};

#define CHECK(cond)                                                               \
  do {                                                                            \
    if (!(cond)) {                                                                \
      std::fprintf(stderr, "CHECK failed (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                                   \
    }                                                                             \
  } while (0)

int main() {
  CHECK(dwg2::video::present_fps.load() == 60);
  CHECK(!dwg2::video::ValidFps(30) && dwg2::video::NormalizeFps(30) == 60);
  CHECK(dwg2::video::NormalizeFps(144) == 60);
  CHECK(dwg2::video::NormalizeFps(0) == 0 && dwg2::video::NormalizeFps(120) == 120);
  CHECK(dwg2::video::HostPacingFps(60, true) == 0);
  CHECK(dwg2::video::HostPacingFps(120, true) == 0);
  CHECK(dwg2::video::HostPacingFps(120, false) == 120);

  // Lock-free pause tick tracking checks.
  CHECK(!dwg2::pause::Requested());
  CHECK(dwg2::pause::ExcludedTicks(1000) == 0);
  CHECK(dwg2::pause::ActiveTicks(1000) == 1000);
  CHECK(dwg2::pause::Set(true, 1500));
  CHECK(dwg2::pause::Requested());
  CHECK(dwg2::pause::ActiveTicks(2000) == 1500);
  CHECK(dwg2::pause::ExcludedTicks(2000) == 500);
  CHECK(dwg2::pause::Set(false, 2500));
  CHECK(!dwg2::pause::Requested());
  CHECK(dwg2::pause::ExcludedTicks(3000) == 1000);
  CHECK(dwg2::pause::ActiveTicks(3000) == 2000);
  // Monotonic clamp when a concurrent reader samples a stale raw clock.
  CHECK(dwg2::pause::ActiveTicks(1900) == 2000);

  // Camera refresh L1 cache & FOV bounds checks.
  dwg2::camera::RefreshTracker tracker;
  CHECK(tracker.NeedsRefresh(0x82001000u, 0));
  CHECK(!tracker.NeedsRefresh(0x82001000u, 0));
  CHECK(tracker.NeedsRefresh(0x82001000u, 25));
  CHECK(!tracker.NeedsRefresh(0x82001000u, 25));
  CHECK(dwg2::camera::ValidPercent(50));
  CHECK(dwg2::camera::ValidPercent(100));
  CHECK(!dwg2::camera::ValidPercent(105));
  CHECK(!dwg2::camera::ValidPercent(52));

  // Startup load / automatic save snapshot gate checks.
  dwg2::startup::LoadSnapshot ls{};
  ls.caller = 0x82176B00;
  ls.mode = 2;
  ls.state = 5;
  ls.buffer = 0x10000000;
  ls.magic = 0x00790001;
  ls.count = 5;
  ls.stride = 0x000A4000;
  ls.occupied = 1;
  ls.data_offset = 64;
  CHECK(dwg2::startup::CanLoadFirstSlot(ls));

  dwg2::saving::Snapshot ss{};
  ss.caller = 0x8227E76C;
  ss.mode = 0;
  ss.state = 5;
  ss.ready = 2;
  ss.buffer = 0x10000000;
  ss.magic = 0x00790001;
  ss.count = 5;
  ss.stride = 0x000A4000;
  ss.data_offset = 64;
  CHECK(dwg2::saving::CanSaveFirstSlot(ss));

  // XMA2 packet offset bounds and 3-stream interleaved skip traversal checks.
  CHECK(!dwg2::xma::HasFrameStart(31));
  CHECK(dwg2::xma::HasFrameStart(32));
  CHECK(dwg2::xma::HasFrameStart(16340u + dwg2::xma::kBitsPerPacketHeader));  // 16372 < 16384
  CHECK(dwg2::xma::HasFrameStart(dwg2::xma::kBitsPerPacket - 1));
  CHECK(!dwg2::xma::HasFrameStart(dwg2::xma::kBitsPerPacket));
  CHECK(!dwg2::xma::HasFrameStart(0x7FFFu + dwg2::xma::kBitsPerPacketHeader));  // 32799

  auto write_xma2_hdr = [](uint8_t* pkt, uint32_t offset_val, uint8_t skip) {
    pkt[0] = uint8_t((1u << 2) | ((offset_val >> 13) & 0x3u));
    pkt[1] = uint8_t((offset_val >> 5) & 0xFFu);
    pkt[2] = uint8_t(((offset_val & 0x1Fu) << 3) | 1u);
    pkt[3] = skip;
  };
  constexpr uint32_t kPkts = 16;
  std::array<uint8_t, kPkts * 2048> block{};
  for (uint32_t i = 0; i < kPkts; ++i) {
    write_xma2_hdr(&block[i * 2048], 64, (i + 3 >= kPkts) ? 0xFF : 2);
  }
  // Stream 1 packets (1, 4, 7, 10, 13): two 0x7FFF continuations then packet 7 frame,
  // ending with valid last-32-bit frame at packet 13 (skip=0xFF).
  write_xma2_hdr(&block[1 * 2048], 0x7FFF, 2);
  write_xma2_hdr(&block[4 * 2048], 0x7FFF, 2);
  write_xma2_hdr(&block[7 * 2048], 128, 2);
  write_xma2_hdr(&block[13 * 2048], 16340, 0xFF);
  CHECK(dwg2::xma::NextPacketIndex(1, 2, kPkts) == 4);
  CHECK(dwg2::xma::FindNextPacketReadOffset(block.data(), 1, kPkts) ==
        7u * dwg2::xma::kBitsPerPacket + 128u + dwg2::xma::kBitsPerPacketHeader);
  CHECK(dwg2::xma::FindNextPacketReadOffset(block.data(), 13, kPkts) ==
        13u * dwg2::xma::kBitsPerPacket + 16340u + dwg2::xma::kBitsPerPacketHeader);
  CHECK(dwg2::xma::NextPacketIndex(13, 0xFF, kPkts) == kPkts);
  CHECK(dwg2::xma::FindNextPacketReadOffset(block.data(), kPkts, kPkts) ==
        dwg2::xma::kBitsPerPacketHeader);
  // Continuation-only final packet with skip=0xFF must terminate without crossing into packet 14/15.
  write_xma2_hdr(&block[13 * 2048], 0x7FFF, 0xFF);
  CHECK(dwg2::xma::FindNextPacketReadOffset(block.data(), 13, kPkts) ==
        dwg2::xma::kBitsPerPacketHeader);

  // Prompt auto-detection and explicit choice override checks.
  dwg2::prompts::choice.store(0);
  dwg2::prompts::last_device.store(1);
  CHECK(dwg2::prompts::Effective() == 1);
  dwg2::prompts::last_device.store(2);
  CHECK(dwg2::prompts::Effective() == 2);
  dwg2::prompts::last_device.store(3);
  CHECK(dwg2::prompts::Effective() == 3);
  dwg2::prompts::choice.store(2);
  dwg2::prompts::last_device.store(1);
  CHECK(dwg2::prompts::Effective() == 2);
  dwg2::prompts::last_device.store(3);
  CHECK(dwg2::prompts::Effective() == 3);
  dwg2::prompts::choice.store(0);
  dwg2::prompts::last_device.store(1);

  // Keyboard diagram BC3 encode and prompt-atlas stamping checks.
  dwg2::keyboard::Bindings kb_primary{};
  dwg2::keyboard::Bindings kb_alt{};
  kb_primary[0] = 'J';
  kb_primary[1] = 'K';
  kb_primary[2] = 'L';
  kb_primary[3] = VK_SPACE;
  kb_alt[0] = VK_RETURN;
  const auto bc3 = dwg2::keyboard::Bc3(dwg2::keyboard::Diagram(kb_primary, kb_alt), 512, 256);
  CHECK(bc3.size() == 131072);
  std::vector<uint8_t> atlas512(size_t(512) * 256 * 4, 0);
  CHECK(dwg2::keyboard::Prompts(atlas512, 512, 256, 2, kb_primary, kb_alt));
  std::vector<uint8_t> atlas1024(size_t(1024) * 512 * 4, 0);
  CHECK(!dwg2::keyboard::Prompts(atlas1024, 1024, 512, 2, kb_primary, kb_alt));
  CHECK(dwg2::keyboard::Prompts(atlas1024, 1024, 512, 3, kb_primary, kb_alt));
  CHECK(dwg2::keyboard::Prompts(atlas1024, 1024, 512, 4, kb_primary, kb_alt));
  return 0;
}
