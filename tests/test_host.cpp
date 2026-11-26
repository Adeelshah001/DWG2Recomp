#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "automatic_save.h"
#include "camera_control.h"
#include "camera_refresh.h"
#include "game_ui_assets.h"
#include "input_config.h"
#include "keyboard_art.h"
#include "player_control_settings.h"
#include "prompt_control.h"
#if __has_include(<stb_image.h>)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include <stb_image.h>
#include "prompt_packs.h"
#define DWG2_HAS_PROMPT_PACK_PNG_TESTS 1
#endif
#include "recomp_menu_layout.h"
#include "runtime_pause.h"
#include "startup_load.h"
#include "video_control.h"
#include "xma_synchronization.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

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

  // A guest safe point must wait for Resume while the host clock keeps excluding the pause.
  {
    using namespace std::chrono_literals;
    CHECK(dwg2::pause::Set(true, 4000));
    std::promise<void> entered, returned;
    auto guest_entered = entered.get_future();
    auto guest_returned = returned.get_future();
    std::thread guest([&] {
      entered.set_value();
      dwg2::pause::Point();
      returned.set_value();
    });
    const bool reached = guest_entered.wait_for(1s) == std::future_status::ready;
    const bool parked = guest_returned.wait_for(30ms) == std::future_status::timeout;
    const auto held_ticks = dwg2::pause::ActiveTicks(1004000);
    const bool resumed = dwg2::pause::Set(false, 1004000);
    guest.join();
    CHECK(reached && parked && resumed);
    CHECK(guest_returned.wait_for(0ms) == std::future_status::ready);
    CHECK(held_ticks == 3000 && dwg2::pause::ActiveTicks(1004010) == held_ticks + 10);
  }

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

  // Prompt auto-detection, SDL3 controller classification, and explicit override checks.
  dwg2::prompts::has_input_activity.store(false);
  dwg2::prompts::controller_seeded.store(false);
  dwg2::prompts::choice.store(0);
  dwg2::prompts::last_device.store(1);
  dwg2::prompts::last_controller.store(1);
  dwg2::prompts::SeedConnectedController(dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::last_controller.load() == dwg2::prompts::kXboxSeries);
  // Seeding happens at most once until real input activity.
  dwg2::prompts::SeedConnectedController(dwg2::prompts::kPs5);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::NotifyActivity(dwg2::prompts::kPs4, true));
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kPs4);
  // Once input activity happened, SeedConnectedController does not clobber active device.
  dwg2::prompts::controller_seeded.store(false);
  dwg2::prompts::SeedConnectedController(dwg2::prompts::kSwitch);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kPs4);
  CHECK(dwg2::prompts::NotifyActivity(dwg2::prompts::kKeyboard, false));
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kKeyboard);
  // Manual choice honors explicit selection 1..8 (including Keyboard).
  dwg2::prompts::choice.store(dwg2::prompts::kPs5);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kPs5);
  dwg2::prompts::choice.store(dwg2::prompts::kSwitch);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kSwitch);
  dwg2::prompts::choice.store(dwg2::prompts::kKeyboard);
  CHECK(dwg2::prompts::Effective() == dwg2::prompts::kKeyboard);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(6, 6, 0x054C, 0x0CE6, true, "DualSense") == dwg2::prompts::kPs5);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(0, 0, 0x054C, 0x0DF2, true, "DualSense Edge") == dwg2::prompts::kPs5);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(5, 5, 0x054C, 0x05C4, false, "DualShock 4") == dwg2::prompts::kPs4);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(4, 4, 0x054C, 0x0268, false, "PS3") == dwg2::prompts::kPs3);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(7, 7, 0x057E, 0x2009, true, "Switch Pro Controller") == dwg2::prompts::kSwitch);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(10, 10, 0x057E, 0x2006, true, "Joy-Con Pair") == dwg2::prompts::kSwitch);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(3, 3, 0x045E, 0x0B12, true, "Xbox Wireless Controller") == dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(3, 3, 0x045E, 0x0B13, true, "Xbox Series BLE") == dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(3, 3, 0x0F0D, 0x0100, true, "Third-party Series") == dwg2::prompts::kXboxSeries);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(3, 3, 0x045E, 0x02EA, false, "Xbox One S") == dwg2::prompts::kXboxOne);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(3, 3, 0x045E, 0x0B00, false, "Xbox Elite 2") == dwg2::prompts::kXboxOne);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(2, 2, 0x045E, 0x028E, false, "X360") == dwg2::prompts::kXbox360);
  CHECK(dwg2::prompts::ClassifyGamepadStyle(1, 1, 0x1234, 0x5678, false, "Generic XInput") == dwg2::prompts::kXbox360);
  dwg2::prompts::choice.store(0);
  dwg2::prompts::last_device.store(1);
  dwg2::prompts::last_controller.store(1);
  dwg2::prompts::has_input_activity.store(false);
  dwg2::prompts::controller_seeded.store(false);

#ifdef DWG2_HAS_PROMPT_PACK_PNG_TESTS
  {
    auto decode_png = [](const uint8_t* data, size_t sz, int& w, int& h) -> std::vector<uint8_t> {
      w = h = 0;
      if (!data || sz == 0) return {};
      int ch = 0;
      stbi_uc* p = stbi_load_from_memory(data, int(sz), &w, &h, &ch, 4);
      if (!p) return {};
      std::vector<uint8_t> out(p, p + size_t(w) * size_t(h) * 4);
      stbi_image_free(p);
      return out;
    };
    CHECK(dwg2::prompt_packs::kPackAssetCount == 317);
    for (size_t i = 0; i < dwg2::prompt_packs::kPackAssetCount; ++i) {
      const auto& a = dwg2::prompt_packs::kPackAssets[i];
      if (i > 0) CHECK(std::string_view(dwg2::prompt_packs::kPackAssets[i - 1].path) < std::string_view(a.path));
      CHECK(dwg2::prompt_packs::FindEmbeddedAsset(a.path) == &a);
      int w = 0, h = 0;
      auto rgba = decode_png(dwg2::prompt_packs::kPackPngBlob + a.offset, a.size, w, h);
      CHECK(w > 0 && h > 0 && rgba.size() == size_t(w) * size_t(h) * 4);
    }

    auto has_nonzero_alpha = [](const std::vector<uint8_t>& buf, int aw, int x, int y, int w, int h) {
      for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
          if (buf[(size_t(y + yy) * size_t(aw) + size_t(x + xx)) * 4 + 3] != 0) return true;
      return false;
    };
    dwg2::keyboard::Bindings def_kb{}, def_kb_alt{};
    const auto kb_prof = dwg2::input::Defaults(0);
    for (int i = 0; i < 25; ++i) {
      def_kb[i] = kb_prof.bind[i].kind == 1 ? unsigned(kb_prof.bind[i].code) : 0u;
      def_kb_alt[i] = kb_prof.alternate[i].kind == 1 ? unsigned(kb_prof.alternate[i].code) : 0u;
    }
    dwg2::prompt_packs::GamepadBindings def_gp{};
    const auto xb_prof = dwg2::input::Defaults(2);
    for (int i = 0; i < 25; ++i) {
      def_gp.kind[i] = xb_prof.bind[i].kind;
      def_gp.code[i] = xb_prof.bind[i].code;
      def_gp.sign[i] = xb_prof.bind[i].sign;
    }

    for (int st : {1, 2, 3, 4, 5, 6, 7, 8}) {
      std::vector<uint8_t> k2(size_t(512) * 256 * 4, 0);
      k2[(size_t(150) * 512 + 10) * 4 + 3] = 200; // simulated backing inside (0,144,108,108)
      CHECK(dwg2::prompt_packs::ApplyPromptAtlas(k2, 512, 256, 2, st, decode_png, {}, def_kb, def_kb_alt, def_gp));
      CHECK(k2[(size_t(150) * 512 + 10) * 4 + 3] == 200);
      if (st != 1) CHECK(has_nonzero_alpha(k2, 512, 72, 72, 72, 72));

      std::vector<uint8_t> k3(size_t(1024) * 512 * 4, 0);
      CHECK(dwg2::prompt_packs::ApplyPromptAtlas(k3, 1024, 512, 3, st, decode_png, {}, def_kb, def_kb_alt, def_gp));
      if (st != 1) CHECK(has_nonzero_alpha(k3, 1024, 422, 177, 53, 27));

      std::vector<uint8_t> k4(size_t(1024) * 512 * 4, 0);
      CHECK(dwg2::prompt_packs::ApplyPromptAtlas(k4, 1024, 512, 4, st, decode_png, {}, def_kb, def_kb_alt, def_gp));
      if (st == 2 || st == 3 || st == 6 || st == 7 || st == 8) {
        CHECK(has_nonzero_alpha(k4, 1024, 688, 170, 44, 27));
      }

      std::vector<uint8_t> k5(131072, 0x5A);
      CHECK(dwg2::prompt_packs::ApplyControllerDiagram(st, decode_png, {}, def_kb, def_kb_alt, k5));
      CHECK(k5.size() == 131072);
      if (st == 1) CHECK(k5[0] == 0x5A);
      else CHECK(k5[0] != 0x5A || k5[8] != 0x5A);
    }

    // Dynamic remapping checks: Keyboard wide key + Switch/Xbox360/PS5 remap + unbound clear.
    auto remap_gp = def_gp;
    remap_gp.kind[0] = 2; remap_gp.code[0] = 9;   // A -> LB
    remap_gp.kind[2] = 0; remap_gp.code[2] = 0;   // X -> Unbound
    remap_gp.kind[9] = 2; remap_gp.code[9] = 20;  // Start -> Touchpad
    std::vector<uint8_t> ps5_k2(size_t(512) * 256 * 4, 123);
    CHECK(dwg2::prompt_packs::ApplyPromptAtlas(ps5_k2, 512, 256, 2, dwg2::prompts::kPs5, decode_png, {}, def_kb, def_kb_alt, remap_gp));
    CHECK(ps5_k2[(size_t(0) * 512 + 396) * 4 + 3] == 0); // unbound X cleared to transparent
    std::vector<uint8_t> ps5_k3(size_t(1024) * 512 * 4, 0);
    CHECK(dwg2::prompt_packs::ApplyPromptAtlas(ps5_k3, 1024, 512, 3, dwg2::prompts::kPs5, decode_png, {}, def_kb, def_kb_alt, remap_gp));
    CHECK(has_nonzero_alpha(ps5_k3, 1024, 422, 177, 53, 27));

    auto remap_kb = def_kb;
    remap_kb[0] = 'K';
    remap_kb[9] = VK_TAB;
    remap_kb[2] = 'L' | 0x100u; // Shift+L -> Keycap fallback
    std::vector<uint8_t> kb_k3(size_t(1024) * 512 * 4, 0);
    CHECK(dwg2::prompt_packs::ApplyPromptAtlas(kb_k3, 1024, 512, 3, dwg2::prompts::kKeyboard, decode_png, {}, remap_kb, def_kb_alt, def_gp));
    CHECK(has_nonzero_alpha(kb_k3, 1024, 297, 249, 28, 27));
    CHECK(has_nonzero_alpha(kb_k3, 1024, 422, 177, 53, 27));
    CHECK(has_nonzero_alpha(kb_k3, 1024, 705, 165, 28, 27));
    std::vector<uint8_t> kb_k5(131072, 0);
    CHECK(dwg2::prompt_packs::ApplyControllerDiagram(dwg2::prompts::kKeyboard, decode_png, {}, remap_kb, def_kb_alt, kb_k5));
  }
#endif

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

  // game_ui_assets: Xbox BC3 decode, G1TG0050 parsing, corrupt/missing fallback, ASCII font coords.
  {
    // 16-byte Xbox BC3 block (16-bit word swapped): a0=255, a1=0, alpha_idx=0, c0=0xF800 (red), c1=0x07E0, color_idx=0.
    const uint8_t bc3_blk[16] = {
        0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xF8, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x00, 0x00,
    };
    std::vector<uint8_t> rgba;
    CHECK(!dwg2::ui_assets::DecodeXboxBc3ToRgba(nullptr, 16, 4, 4, rgba));
    CHECK(!dwg2::ui_assets::DecodeXboxBc3ToRgba(bc3_blk, 8, 4, 4, rgba));
    CHECK(dwg2::ui_assets::DecodeXboxBc3ToRgba(bc3_blk, sizeof(bc3_blk), 4, 4, rgba));
    CHECK(rgba.size() == 64);
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
    CHECK(rgba[60] == 255 && rgba[61] == 0 && rgba[62] == 0 && rgba[63] == 255);

    std::vector<uint8_t> g1t(28 + 4 + 8 + 16, 0);
    std::memcpy(g1t.data(), "G1TG0050", 8);
    auto put_be32 = [&](size_t off, uint32_t v) {
      g1t[off + 0] = uint8_t(v >> 24);
      g1t[off + 1] = uint8_t(v >> 16);
      g1t[off + 2] = uint8_t(v >> 8);
      g1t[off + 3] = uint8_t(v);
    };
    put_be32(8, uint32_t(g1t.size()));
    put_be32(12, 28);  // head
    put_be32(16, 1);   // n
    put_be32(28, 4);   // relative offset from head
    g1t[32 + 1] = 8;   // fmt = BC3
    g1t[32 + 2] = 0x22;  // 4x4
    std::memcpy(g1t.data() + 40, bc3_blk, 16);

    dwg2::ui_assets::Image parsed;
    CHECK(dwg2::ui_assets::ParseLinkdataG1t(g1t, 0, 0, parsed));
    CHECK(parsed.width == 4 && parsed.height == 4 && parsed.rgba.size() == 64);
    CHECK(parsed.rgba[0] == 255 && parsed.rgba[1] == 0 && parsed.rgba[2] == 0 && parsed.rgba[3] == 255);
    CHECK(!dwg2::ui_assets::ParseLinkdataG1t(g1t, 0, 1, parsed));
    g1t[0] = 'X';
    CHECK(!dwg2::ui_assets::ParseLinkdataG1t(g1t, 0, 0, parsed));

    const auto missing = std::filesystem::temp_directory_path() / L"dwg2_missing_linkdata_test.bns";
    std::error_code ec;
    std::filesystem::remove(missing, ec);
    const auto fb_missing = dwg2::ui_assets::LoadPack(missing);
    CHECK(!fb_missing.from_game_data);
    for (int t = 0; t < dwg2::ui_assets::kTexCount; ++t) {
      CHECK(fb_missing.textures[t].width > 0 && fb_missing.textures[t].height > 0);
      CHECK(!fb_missing.textures[t].rgba.empty());
    }
    CHECK(fb_missing.ascii_glyphs[' '].valid && fb_missing.ascii_glyphs[' '].advance == 10);
    CHECK(fb_missing.ascii_glyphs['0'].valid && fb_missing.ascii_glyphs['0'].cell_x == 0 &&
          fb_missing.ascii_glyphs['0'].cell_y == 1 * 28);
    CHECK(fb_missing.ascii_glyphs['A'].valid && fb_missing.ascii_glyphs['A'].cell_x == 1 * 28 &&
          fb_missing.ascii_glyphs['A'].cell_y == 2 * 28);
    CHECK(fb_missing.ascii_glyphs['Z'].valid && fb_missing.ascii_glyphs['Z'].cell_x == 10 * 28 &&
          fb_missing.ascii_glyphs['Z'].cell_y == 3 * 28);
    CHECK(fb_missing.ascii_glyphs['a'].valid && fb_missing.ascii_glyphs['a'].cell_x == 1 * 28 &&
          fb_missing.ascii_glyphs['a'].cell_y == 4 * 28);
    CHECK(fb_missing.ascii_glyphs['['].valid && fb_missing.ascii_glyphs['['].cell_x == 11 * 28 &&
          fb_missing.ascii_glyphs['['].cell_y == 3 * 28);
    CHECK(fb_missing.ascii_glyphs['%'].valid && fb_missing.ascii_glyphs['%'].cell_x == 5 * 28 &&
          fb_missing.ascii_glyphs['%'].cell_y == 0);
    CHECK(fb_missing.ascii_glyphs['A'].advance >= 6 && fb_missing.ascii_glyphs['A'].advance <= 28);

    // UTF-8 decoding and Latin-1 / European accented glyph table checks.
    {
      const std::string_view sample = "éèäöüßñçàìœŒ¡¿";
      size_t idx = 0;
      const uint32_t expected_cps[] = {
          0x00E9u, 0x00E8u, 0x00E4u, 0x00F6u, 0x00FCu, 0x00DFu, 0x00F1u,
          0x00E7u, 0x00E0u, 0x00ECu, 0x0153u, 0x0152u, 0x00A1u, 0x00BFu};
      for (uint32_t exp : expected_cps) {
        const uint32_t cp = dwg2::ui_assets::NextUtf8Codepoint(sample, idx);
        CHECK(cp == exp);
        CHECK(fb_missing.HasGlyph(cp));
      }
      CHECK(idx == sample.size());
      CHECK(fb_missing.GlyphForCodepoint(0x00E9u).cell_x == 13 * 28 &&
            fb_missing.GlyphForCodepoint(0x00E9u).cell_y == 6 * 28);
      CHECK(fb_missing.GlyphForCodepoint(0x0153u).cell_x == 5 * 28 &&
            fb_missing.GlyphForCodepoint(0x0153u).cell_y == 7 * 28);
      CHECK(fb_missing.GlyphForCodepoint(0x00BFu).cell_x == 0 &&
            fb_missing.GlyphForCodepoint(0x00BFu).cell_y == 9 * 28);
    }

    const auto corrupt = std::filesystem::temp_directory_path() / L"dwg2_corrupt_linkdata_test.bns";
    {
      std::ofstream out(corrupt, std::ios::binary | std::ios::trunc);
      const char garbage[32] = "NOT_A_VALID_LINKDATA_FILE_00000";
      out.write(garbage, sizeof(garbage));
    }
    const auto fb_corrupt = dwg2::ui_assets::LoadPack(corrupt);
    std::filesystem::remove(corrupt, ec);
    CHECK(!fb_corrupt.from_game_data);
    CHECK(fb_corrupt.textures[dwg2::ui_assets::kTexFont].width == 512);
  }

  // input_config & player_control_settings: RecompMenu defaults, legacy migration, guest LT suppression.
  {
    const auto kb_def = dwg2::input::Defaults(0);
    const auto ps_def = dwg2::input::Defaults(1);
    const auto xb_def = dwg2::input::Defaults(2);
    CHECK(dwg2::input::RecompMenu == 25 && dwg2::input::Count == 26);
    CHECK((kb_def.bind[dwg2::input::RecompMenu] == dwg2::input::Binding{1, 112, 1, 0}));
    CHECK((ps_def.bind[dwg2::input::RecompMenu] == dwg2::input::Binding{3, 4, 1, 0}));
    CHECK((ps_def.alternate[dwg2::input::RecompMenu] == dwg2::input::Binding{2, 20, 1, 0}));
    CHECK((ps_def.bind[dwg2::input::Fullscreen] == dwg2::input::Binding{}));
    CHECK((xb_def.bind[dwg2::input::RecompMenu] == dwg2::input::Binding{3, 4, 1, 0}));
    CHECK((xb_def.bind[dwg2::input::Fullscreen] == dwg2::input::Binding{}));

    dwg2::input::Raw raw_lt{};
    raw_lt.axes[4] = 1.0f;
    const auto mapped_xb = dwg2::input::Map(xb_def, raw_lt);
    CHECK(mapped_xb.recomp_menu && mapped_xb.recomp_menu_level >= 0.65f);
    CHECK(mapped_xb.lt == 0);

    dwg2::input::Raw raw_tp{};
    raw_tp.buttons[20] = true;
    const auto mapped_ps = dwg2::input::Map(ps_def, raw_tp);
    CHECK(mapped_ps.recomp_menu && mapped_ps.buttons == 0);

    dwg2::settings::Document doc_legacy;
    CHECK(doc_legacy.Parse(
        "[controls.playstation]\n"
        "fullscreen = 3,4,1,0\n"
        "fullscreen.alternate = 2,20,1,0\n"
        "[controls.xbox]\n"
        "fullscreen = 3,4,1,0\n"));
    std::array<dwg2::input::Profile, 3> mig = {kb_def, ps_def, xb_def};
    CHECK(dwg2::input::LoadPlayerControls(doc_legacy, mig));
    CHECK((mig[1].bind[dwg2::input::RecompMenu] == dwg2::input::Binding{3, 4, 1, 0}));
    CHECK((mig[1].alternate[dwg2::input::RecompMenu] == dwg2::input::Binding{2, 20, 1, 0}));
    CHECK((mig[1].bind[dwg2::input::Fullscreen] == dwg2::input::Binding{}));
    CHECK((mig[1].alternate[dwg2::input::Fullscreen] == dwg2::input::Binding{}));
    CHECK((mig[2].bind[dwg2::input::RecompMenu] == dwg2::input::Binding{3, 4, 1, 0}));
    CHECK((mig[2].bind[dwg2::input::Fullscreen] == dwg2::input::Binding{}));

    dwg2::settings::Document doc_custom;
    CHECK(doc_custom.Parse(
        "[controls.playstation]\n"
        "fullscreen = 2,15,1,0\n"
        "[controls.xbox]\n"
        "fullscreen = 3,4,1,0\n"
        "recomp_menu = 2,8,1,0\n"));
    std::array<dwg2::input::Profile, 3> cust = {kb_def, ps_def, xb_def};
    CHECK(dwg2::input::LoadPlayerControls(doc_custom, cust));
    CHECK((cust[1].bind[dwg2::input::Fullscreen] == dwg2::input::Binding{2, 15, 1, 0}));
    CHECK((cust[2].bind[dwg2::input::Fullscreen] == dwg2::input::Binding{3, 4, 1, 0}));
    CHECK((cust[2].bind[dwg2::input::RecompMenu] == dwg2::input::Binding{2, 8, 1, 0}));
  }

  // recomp_menu_state: open/close, 12-item nav, actions, quit modal, controls wizard, mouse hit-test.
  {
    const std::array<dwg2::input::Profile, 3> saved_profiles = {
        dwg2::input::Get(0), dwg2::input::Get(1), dwg2::input::Get(2)};
    bool m_paused = false, m_full = false, m_fps_cnt = false, m_vsync = false, m_turbo = false,
         m_quit = false;
    int m_res = 1, m_fov = 0, m_fps = 60, m_prompts = 0, m_lang = 1, m_save_calls = 0,
        m_restarts = 0, m_lang_restarts = 0, m_update_checks = 0, m_turbo_set_calls = 0;

    dwg2::recomp_menu::Actions act{};
    act.restart = [&]() -> std::string { ++m_restarts; return "Restart unavailable."; };
    act.check_updates = [&]() -> std::string { ++m_update_checks; return "Checking for updates..."; };
    act.toggle_pause = [&]() -> std::string { m_paused = !m_paused; return {}; };
    act.quit_game = [&]() { m_quit = true; };
    act.set_fullscreen = [&](bool v) -> std::string { m_full = v; return {}; };
    act.set_resolution = [&](int r) -> std::string { m_res = r; return {}; };
    act.set_fov = [&](int f) -> std::string { m_fov = f; return {}; };
    act.set_fps = [&](int f) -> std::string { m_fps = f; return {}; };
    act.set_fps_counter = [&](bool v) -> std::string { m_fps_cnt = v; return {}; };
    act.set_vsync = [&](bool v) -> std::string { m_vsync = v; return {}; };
    act.set_turbo = [&](bool v) -> std::string { ++m_turbo_set_calls; m_turbo = v; return {}; };
    act.set_prompts = [&](int p) -> std::string { m_prompts = p; return {}; };
    act.apply_language = [&](int lang) -> std::string {
      m_lang = lang;
      ++m_lang_restarts;
      return {};
    };
    act.save_controls = [&](const std::array<dwg2::input::Profile, 3>&) -> std::string {
      ++m_save_calls;
      return {};
    };
    act.is_paused = [&]() { return m_paused; };
    act.is_fullscreen = [&]() { return m_full; };
    act.resolution = [&]() { return m_res; };
    act.fov = [&]() { return m_fov; };
    act.fps = [&]() { return m_fps; };
    act.fps_counter = [&]() { return m_fps_cnt; };
    act.vsync = [&]() { return m_vsync; };
    act.turbo = [&]() { return m_turbo; };
    act.prompt_choice = [&]() { return m_prompts; };
    act.language = [&]() { return m_lang; };

    CHECK(dwg2::recomp_menu::kMainItemCount == 14);
    dwg2::recomp_menu::SetOpen(false);
    CHECK(!dwg2::recomp_menu::IsOpen());
    dwg2::recomp_menu::Toggle(false);
    CHECK(dwg2::recomp_menu::IsOpen());
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);

    uint64_t now = 100;
    auto kb_tap = [&](int vk) {
      dwg2::input::Raw r{};
      r.keys[vk] = true;
      dwg2::recomp_menu::HandleKeyboard(r, now, act);
      now += 20;
      dwg2::recomp_menu::HandleKeyboard(dwg2::input::Raw{}, now, act);
      now += 20;
    };

    kb_tap(13);  // Pause/Resume on left col (0)
    CHECK(m_paused && dwg2::recomp_menu::Snapshot(act).paused);
    kb_tap(13);
    CHECK(!m_paused && dwg2::recomp_menu::IsOpen());
    kb_tap(39);  // Pause -> Restart (middle col 1)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);
    kb_tap(13);
    auto restart = dwg2::recomp_menu::Snapshot(act);
    CHECK(restart.view == dwg2::recomp_menu::View::kConfirmRestart && !restart.confirm_quit_yes);
    kb_tap(13);
    CHECK(m_restarts == 0 && dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    kb_tap(13);
    kb_tap(27);
    CHECK(m_restarts == 0 && dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);
    kb_tap(13);
    kb_tap(37);
    kb_tap(13);
    CHECK(m_restarts == 1 && dwg2::recomp_menu::IsOpen());
    CHECK(dwg2::recomp_menu::Snapshot(act).toast == "Restart unavailable.");
    // Row-1 left/right wrap + up/down row navigation (Pause | Restart | Quit Game).
    kb_tap(39);  // Restart -> Quit Game
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kQuitGame);
    kb_tap(39);  // Quit Game -> Pause (wrap right)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    kb_tap(37);  // Pause -> Quit Game (wrap left)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kQuitGame);
    kb_tap(37);  // Quit Game -> Restart
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);
    kb_tap(37);  // Restart -> Pause
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    kb_tap(40);  // Pause -> Fullscreen (row 2)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kFullscreen);
    kb_tap(38);  // Fullscreen -> Pause
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    kb_tap(37);  // Pause -> Quit Game
    kb_tap(38);  // Quit Game -> Check for Updates (bottom row)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kCheckUpdates);
    kb_tap(13);
    CHECK(m_update_checks == 1 && dwg2::recomp_menu::Snapshot(act).toast == "Checking for updates...");
    kb_tap(40);  // Check for Updates -> Pause (wrap down)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    kb_tap(40);  // Pause -> Fullscreen
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kFullscreen);
    kb_tap(38);  // Fullscreen -> Pause
    kb_tap(39);  // Pause -> Restart
    kb_tap(38);  // Restart -> Check for Updates
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kCheckUpdates);
    kb_tap(40);  // Check for Updates -> Pause
    kb_tap(38);  // Pause -> Check for Updates (wrap up)
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kCheckUpdates);
    kb_tap(40);  // Back to Pause

    // Walk all 12 main rows (Pause, Fullscreen..CheckUpdates) and verify automatic main_scroll
    // tracking plus top<->bottom wrap.
    CHECK(dwg2::recomp_menu::MaxMainScroll() > 0);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 0);
    kb_tap(40);
    for (int i = dwg2::recomp_menu::kFullscreen; i < dwg2::recomp_menu::kMainItemCount; ++i) {
      const auto s_walk = dwg2::recomp_menu::Snapshot(act);
      CHECK(s_walk.focus == i);
      const int row = dwg2::recomp_menu::FocusToMainRow(i);
      CHECK(row >= s_walk.main_scroll && row <= dwg2::recomp_menu::LastVisibleMainRow(s_walk.main_scroll));
      kb_tap(40);  // Down
    }
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 0);
    kb_tap(38);  // Wrap Up Pause -> CheckUpdates
    {
      const auto s_bot = dwg2::recomp_menu::Snapshot(act);
      CHECK(s_bot.focus == dwg2::recomp_menu::kCheckUpdates);
      CHECK(s_bot.main_scroll == dwg2::recomp_menu::MaxMainScroll());
      CHECK(dwg2::recomp_menu::LastVisibleMainRow(s_bot.main_scroll) ==
            dwg2::recomp_menu::kMainRowCount - 1);
    }
    kb_tap(40);  // Wrap Down CheckUpdates -> Pause
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kPause);
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 0);

    // Resolution, FOV, FPS, FPS Counter, VSync, Prompts actions.
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kResolution) kb_tap(40);
    kb_tap(39);  // Right -> 2
    CHECK(m_res == 2);
    kb_tap(39);  // Right -> 3
    CHECK(m_res == 3);
    kb_tap(40);  // FOV
    kb_tap(39);  // +5%
    CHECK(m_fov == 5);
    kb_tap(40);  // FPS
    kb_tap(13);  // 60 -> 120
    CHECK(m_fps == 120);
    kb_tap(13);  // 120 -> 0
    CHECK(m_fps == 0);
    kb_tap(40);  // FPS Counter
    kb_tap(13);
    CHECK(m_fps_cnt);
    kb_tap(40);  // VSync
    kb_tap(13);
    CHECK(m_vsync);
    kb_tap(40);  // Turbo Mode: keyboard, no-op repeat guard, gamepad, stick snapback & cross-source dedup
    CHECK(!m_turbo && m_turbo_set_calls == 0);
    kb_tap(13);  // false -> true
    CHECK(m_turbo && dwg2::recomp_menu::Snapshot(act).turbo && m_turbo_set_calls == 1);
    kb_tap(13);  // true -> false
    CHECK(!m_turbo && !dwg2::recomp_menu::Snapshot(act).turbo && m_turbo_set_calls == 2);
    kb_tap(39);  // Right -> On
    CHECK(m_turbo && m_turbo_set_calls == 3);
    kb_tap(39);  // Right again -> no-op
    CHECK(m_turbo && m_turbo_set_calls == 3);
    kb_tap(37);  // Left -> Off
    CHECK(!m_turbo && m_turbo_set_calls == 4);
    kb_tap(37);  // Left again -> no-op
    CHECK(!m_turbo && m_turbo_set_calls == 4);
    {
      auto gp_btn = [&](int btn) {
        dwg2::input::Raw g{};
        g.buttons[btn] = true;
        dwg2::recomp_menu::HandleGamepad(g, 2, now, act);
        now += 20;
        dwg2::recomp_menu::HandleGamepad(dwg2::input::Raw{}, 2, now, act);
        now += 20;
      };
      gp_btn(0);  // Gamepad A: false -> true
      CHECK(m_turbo && m_turbo_set_calls == 5);
      gp_btn(0);  // Gamepad A: true -> false
      CHECK(!m_turbo && m_turbo_set_calls == 6);
      gp_btn(14); // D-pad Right -> On
      CHECK(m_turbo && m_turbo_set_calls == 7);
      gp_btn(13); // D-pad Left -> Off
      CHECK(!m_turbo && m_turbo_set_calls == 8);
      gp_btn(14); // D-pad Right -> On for stick Left + snapback test
      CHECK(m_turbo && m_turbo_set_calls == 9);
      dwg2::input::Raw st{};
      st.axes[0] = -0.9f;  // Flick Left Stick left -> Off
      dwg2::recomp_menu::HandleGamepad(st, 2, now, act);
      CHECK(!m_turbo && m_turbo_set_calls == 10);
      now += 20;
      dwg2::recomp_menu::HandleGamepad(dwg2::input::Raw{}, 2, now, act);  // Release
      now += 80;
      st.axes[0] = 0.8f;   // Snapback right within 150ms must NOT flip Turbo back On
      dwg2::recomp_menu::HandleGamepad(st, 2, now, act);
      CHECK(!m_turbo && m_turbo_set_calls == 10);
      dwg2::recomp_menu::HandleGamepad(dwg2::input::Raw{}, 2, now + 10, act);
      now += 200;
      dwg2::recomp_menu::HandleGamepad(st, 2, now, act);  // Deliberate stick right after 200ms -> On
      CHECK(m_turbo && m_turbo_set_calls == 11);
      dwg2::recomp_menu::HandleGamepad(dwg2::input::Raw{}, 2, now + 10, act);
      now += 100;
      // Cross-source Enter + A within 20ms must toggle Turbo ONCE (true -> false).
      {
        dwg2::input::Raw kr{}, gr{};
        kr.keys[13] = true;
        gr.buttons[0] = true;
        dwg2::recomp_menu::HandleKeyboard(kr, now, act);
        dwg2::recomp_menu::HandleGamepad(gr, 2, now + 8, act);
        dwg2::recomp_menu::HandleKeyboard(dwg2::input::Raw{}, now + 20, act);
        dwg2::recomp_menu::HandleGamepad(dwg2::input::Raw{}, 2, now + 20, act);
        now += 120;
        CHECK(!m_turbo && m_turbo_set_calls == 12);
      }
      // Re-enable Turbo via Right for Button Prompts step next.
      kb_tap(39);
      CHECK(m_turbo && m_turbo_set_calls == 13);
    }
    {
      const auto tmp_ini = std::filesystem::temp_directory_path() / L"dwg2_turbo_settings_test.ini";
      std::error_code ec;
      std::filesystem::remove(tmp_ini, ec);
      {
        std::ofstream out(tmp_ini, std::ios::binary | std::ios::trunc);
        out << "[video]\nfps_counter = 0\nturbo = 1\n[prompts]\nstyle = 0\n";
      }
      for (int expected : {0, 1, 0}) {
        CHECK(dwg2::settings::Update(tmp_ini, {{"video/turbo", std::to_string(expected)}}));
        dwg2::settings::Document doc;
        CHECK(dwg2::settings::Read(tmp_ini, doc));
        CHECK(dwg2::settings::Integer(doc, "video", "turbo", -1, 0, 1) == expected);
      }
      std::filesystem::remove(tmp_ini, ec);
    }
    kb_tap(40);  // Button Prompts
    for (int expected_style : {1, 4, 5, 7, 6, 2, 8, 3, 0}) {
      kb_tap(13);
      CHECK(m_prompts == expected_style);
    }
    kb_tap(13);
    CHECK(m_prompts == 1);

    // Language selector: Confirm on unchanged language stays in kMain; left/right cycle
    // pending_language without restarting; Confirm on changed language opens Restart Required
    // modal (default No); No/Esc cancels and reverts; Yes applies & restarts.
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kLanguage) kb_tap(40);
    kb_tap(13);  // Unchanged language (1 == 1) -> stays in kMain
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    for (int expected_lang : {4, 3, 6, 5, 1}) {
      kb_tap(39);
      CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == expected_lang);
      CHECK(m_lang == 1 && m_lang_restarts == 0);
    }
    kb_tap(13);  // Wrapped back to 1 (unchanged) -> stays in kMain
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    kb_tap(37);  // Wrap left 1 -> 5
    CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == 5);
    kb_tap(13);  // Confirm -> Restart Required modal (default No)
    {
      auto lsnap = dwg2::recomp_menu::Snapshot(act);
      CHECK(lsnap.view == dwg2::recomp_menu::View::kConfirmLanguageRestart && !lsnap.confirm_quit_yes);
    }
    kb_tap(13);  // Confirm No -> returns to kMain, reverts pending_language
    CHECK(m_lang_restarts == 0 && m_lang == 1);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == 1);
    kb_tap(39);  // 1 -> 4
    kb_tap(39);  // 4 -> 3
    CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == 3);
    kb_tap(13);  // Open modal
    kb_tap(27);  // Esc -> cancels & reverts
    CHECK(m_lang_restarts == 0 && dwg2::recomp_menu::Snapshot(act).pending_language == 1);
    kb_tap(39);  // 1 -> 4
    kb_tap(39);  // 4 -> 3
    kb_tap(13);  // Open modal
    kb_tap(37);  // Left -> Yes
    CHECK(dwg2::recomp_menu::Snapshot(act).confirm_quit_yes);
    kb_tap(13);  // Confirm Yes -> applies language 3 & triggers restart
    CHECK(m_lang == 3 && m_lang_restarts == 1);
    m_lang = 1;  // Reset for remaining English toast checks below.
    dwg2::recomp_menu::SetOpen(false);
    dwg2::recomp_menu::SetOpen(true, false);

    // Inline Quit Yes/No (default No): Up reaches Row 1 (Pause), Left wraps to Quit Game.
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kPause) kb_tap(38);
    kb_tap(37);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kQuitGame);
    kb_tap(13);  // Open ConfirmQuit
    auto snap = dwg2::recomp_menu::Snapshot(act);
    CHECK(snap.view == dwg2::recomp_menu::View::kConfirmQuit && !snap.confirm_quit_yes);
    kb_tap(13);  // Confirm No -> returns to kMain, does not quit
    CHECK(!m_quit && dwg2::recomp_menu::IsOpen());
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    kb_tap(13);  // Re-open ConfirmQuit
    kb_tap(37);  // Left -> Yes
    CHECK(dwg2::recomp_menu::Snapshot(act).confirm_quit_yes);
    kb_tap(13);  // Confirm Yes -> quits and closes
    CHECK(m_quit && !dwg2::recomp_menu::IsOpen());

    // Controls Wizard: profile switch, capture, cancel, clear, deadzone, defaults, cancel-back, save.
    dwg2::recomp_menu::SetOpen(true, false);
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kConfigureControls) kb_tap(40);
    kb_tap(13);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kControlsWizard);
    kb_tap(39);  // Switch profile 0 -> 1
    CHECK(dwg2::recomp_menu::Snapshot(act).wiz_profile == 1);
    kb_tap(37);  // Back to 0
    CHECK(dwg2::recomp_menu::Snapshot(act).wiz_profile == 0);

    // Capture keyboard action 0 (A) -> 'J', then cancel capture on action 1 with Esc.
    kb_tap(13);
    CHECK(dwg2::recomp_menu::IsCapturing());
    kb_tap('J');
    CHECK(!dwg2::recomp_menu::IsCapturing());
    CHECK((dwg2::recomp_menu::Snapshot(act).working_profiles[0].bind[0] ==
           dwg2::input::Binding{1, 'J', 1, 0}));
    kb_tap(40);  // row 1 (B)
    kb_tap(13);
    CHECK(dwg2::recomp_menu::IsCapturing());
    kb_tap(27);  // Esc cancels capture
    CHECK(!dwg2::recomp_menu::IsCapturing());
    CHECK((dwg2::recomp_menu::Snapshot(act).working_profiles[0].bind[1] ==
           saved_profiles[0].bind[1]));
    kb_tap(46);  // Delete clears row 1
    CHECK((dwg2::recomp_menu::Snapshot(act).working_profiles[0].bind[1] ==
           dwg2::input::Binding{}));

    // Deadzone + Restore Defaults + Esc cancel (no save).
    kb_tap(27);  // Esc from wizard -> kMain without saving
    CHECK(m_save_calls == 0);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);

    // Re-enter wizard naturally from kConfigureControls, adjust deadzone, restore defaults, save.
    kb_tap(13);
    while (dwg2::recomp_menu::Snapshot(act).wiz_cursor != dwg2::recomp_menu::kWizDeadzone) kb_tap(38);
    kb_tap(39);  // deadzone 0 -> 5
    CHECK(dwg2::recomp_menu::Snapshot(act).working_profiles[0].deadzone == 5);
    kb_tap(40);  // kWizDefaults
    kb_tap(13);
    CHECK(dwg2::recomp_menu::Snapshot(act).working_profiles[0].deadzone == 0);
    kb_tap(39);  // kWizSave
    kb_tap(13);
    CHECK(m_save_calls == 1);

    // Mouse hit-testing & virtual coordinate mapping.
    float vx = 0, vy = 0;
    CHECK(dwg2::recomp_menu::ScreenToVirtual(960.0f, 540.0f, 1920.0f, 1080.0f, vx, vy));
    CHECK(vx > 639.0f && vx < 641.0f && vy > 359.0f && vy < 361.0f);
    CHECK(dwg2::recomp_menu::ScreenToVirtual(800.0f, 10.0f, 1600.0f, 1200.0f, vx, vy));
    CHECK(!dwg2::recomp_menu::ScreenToVirtual(400.0f, 10.0f, 800.0f, 1000.0f, vx, vy));

    // Returning via kHitWizBack when main_scroll is 0 scrolls kConfigureControls into view.
    {
      auto& st = dwg2::recomp_menu::detail::GetState();
      std::lock_guard lock(st.mutex);
      st.main_scroll = 0;
    }
    dwg2::recomp_menu::UpdateHitRects({
        {50, 100, 90, 120, dwg2::recomp_menu::kHitWizBack, 0},
    }, dwg2::recomp_menu::View::kControlsWizard, 0, dwg2::recomp_menu::Snapshot(act).wiz_scroll);
    CHECK(dwg2::recomp_menu::HandleMouseDown(70.0f, 110.0f, act, now));
    {
      const auto s_back = dwg2::recomp_menu::Snapshot(act);
      CHECK(s_back.view == dwg2::recomp_menu::View::kMain);
      CHECK(s_back.focus == dwg2::recomp_menu::kConfigureControls);
      const int cfg_row = dwg2::recomp_menu::FocusToMainRow(dwg2::recomp_menu::kConfigureControls);
      CHECK(cfg_row >= s_back.main_scroll &&
            cfg_row <= dwg2::recomp_menu::LastVisibleMainRow(s_back.main_scroll));
    }
    // Reset to top scroll and verify mouse hits + wheel + scroll-arrow clicks + focus clamping.
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kPause) kb_tap(38);
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 0);
    dwg2::recomp_menu::UpdateHitRects({
        {100, 100, 300, 120, dwg2::recomp_menu::kHitFovSlider, 0},
        {320, 100, 360, 120, dwg2::recomp_menu::kHitResStop, 1},
        {400, 100, 440, 120, dwg2::recomp_menu::kHitClose, 0},
        {460, 100, 500, 120, dwg2::recomp_menu::kHitToggleOn, dwg2::recomp_menu::kFullscreen},
        {520, 100, 560, 120, dwg2::recomp_menu::kHitToggleOff, dwg2::recomp_menu::kFullscreen},
        {650, 100, 690, 120, dwg2::recomp_menu::kHitToggleOn, dwg2::recomp_menu::kTurbo},
        {700, 100, 740, 120, dwg2::recomp_menu::kHitToggleOff, dwg2::recomp_menu::kTurbo},
        {580, 100, 640, 120, dwg2::recomp_menu::kHitMainRow, dwg2::recomp_menu::kPause},
    }, dwg2::recomp_menu::View::kMain, 0, 0);
    CHECK(dwg2::recomp_menu::HandleMouseDown(200.0f, 110.0f, act, now));
    CHECK(m_fov == 50);
    dwg2::recomp_menu::HandleMouseMove(250.0f, 110.0f, act);
    CHECK(m_fov == 75);
    dwg2::recomp_menu::HandleMouseUp();
    CHECK(dwg2::recomp_menu::HandleMouseDown(340.0f, 110.0f, act, now));
    CHECK(m_res == 1);
    // Wheel scrolling in View::kMain steps main_scroll, clamps off-screen focus into view,
    // and invalidates stale hit rects until updated.
    dwg2::recomp_menu::HandleMouseWheel(-1.0f);
    {
      const auto sw1 = dwg2::recomp_menu::Snapshot(act);
      CHECK(sw1.main_scroll == 1);
      CHECK(sw1.focus == dwg2::recomp_menu::kResolution);  // Row 2 stays in [1..9]
    }
    CHECK(!dwg2::recomp_menu::HandleMouseDown(480.0f, 110.0f, act, now));  // Stale hit_main_scroll!
    for (int s = 0; s < dwg2::recomp_menu::MaxMainScroll() + 2; ++s) {
      dwg2::recomp_menu::HandleMouseWheel(-1.0f);
    }
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == dwg2::recomp_menu::MaxMainScroll());
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kFov);  // Clamped 2 -> 3
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kCheckUpdates) kb_tap(40);
    dwg2::recomp_menu::HandleMouseWheel(1.0f);  // Scroll up 1 -> clamps kCheckUpdates to kLanguage
    {
      const auto sw_up = dwg2::recomp_menu::Snapshot(act);
      CHECK(sw_up.main_scroll == dwg2::recomp_menu::MaxMainScroll() - 1);
      CHECK(sw_up.focus == dwg2::recomp_menu::kLanguage);
    }
    for (int s = 0; s < dwg2::recomp_menu::MaxMainScroll() + 2; ++s) {
      dwg2::recomp_menu::HandleMouseWheel(1.0f);
    }
    CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 0);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kButtonPrompts);
    CHECK(dwg2::recomp_menu::HandleMouseDown(480.0f, 110.0f, act, now) && m_full);
    CHECK(dwg2::recomp_menu::HandleMouseDown(480.0f, 110.0f, act, now) && m_full);
    CHECK(dwg2::recomp_menu::HandleMouseDown(540.0f, 110.0f, act, now) && !m_full);
    {
      CHECK(m_turbo);
      const int calls_before = m_turbo_set_calls;
      CHECK(dwg2::recomp_menu::HandleMouseDown(670.0f, 110.0f, act, now) && m_turbo);
      CHECK(m_turbo_set_calls == calls_before);  // Already On -> no-op
      CHECK(dwg2::recomp_menu::HandleMouseDown(720.0f, 110.0f, act, now) && !m_turbo);
      CHECK(dwg2::recomp_menu::HandleMouseDown(720.0f, 110.0f, act, now) && !m_turbo);
      CHECK(m_turbo_set_calls == calls_before + 1);  // Second Off -> no-op
      CHECK(dwg2::recomp_menu::HandleMouseDown(670.0f, 110.0f, act, now) && m_turbo);
      CHECK(dwg2::recomp_menu::HandleMouseDown(720.0f, 110.0f, act, now) && !m_turbo);
      CHECK(m_turbo_set_calls == calls_before + 3);
    }
    CHECK(dwg2::recomp_menu::HandleMouseDown(600.0f, 110.0f, act, now) && m_paused);
    CHECK(dwg2::recomp_menu::HandleMouseDown(600.0f, 110.0f, act, now) && !m_paused);
    // Re-select kRestart cleanly for Restart confirm modal test below.
    kb_tap(39);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);
    kb_tap(13);
    CHECK(!dwg2::recomp_menu::HandleMouseDown(420.0f, 110.0f, act, now)); // Stale main hit.
    // Wheel is ignored while a confirm modal is active.
    {
      const int modal_scroll = dwg2::recomp_menu::Snapshot(act).main_scroll;
      dwg2::recomp_menu::HandleMouseWheel(-1.0f);
      CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kConfirmRestart);
      CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == modal_scroll);
    }

    std::vector<dwg2::recomp_menu::HitRect> modal_hits;
    restart = dwg2::recomp_menu::Snapshot(act);
    const auto viewport = dwg2::recomp_menu::ComputeViewport(1280, 800);
    dwg2::recomp_menu::DrawMenuLayout(restart, viewport.virt_w, "",
        [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {}, [](auto...) {},
        [&](auto box, int id, int sub) {
          modal_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
        });
    CHECK(modal_hits.size() == 2 && modal_hits.front().id == dwg2::recomp_menu::kHitQuitYes);
    dwg2::recomp_menu::UpdateHitRects(modal_hits, restart.view, restart.main_scroll, 0);
    const auto yes = modal_hits.front();
    CHECK(dwg2::recomp_menu::ScreenToVirtual(
        viewport.ox + (yes.x0 + yes.x1) / 2 * viewport.scale,
        viewport.oy + (yes.y0 + yes.y1) / 2 * viewport.scale, 1280, 800, vx, vy));
    CHECK(dwg2::recomp_menu::HandleMouseDown(vx, vy, act, now) && m_restarts == 2);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);

    // Scroll indicator hit rects & clicks: top scroll shows only Down arrow; scrolling to bottom
    // shows only Up arrow; clicking arrows steps main_scroll cleanly.
    // Also verify Button Prompts center click & Language center/arrow clicks at scrolled bottom!
    {
      auto find_hit_in = [](const std::vector<dwg2::recomp_menu::HitRect>& hits, int id,
                            int sub) -> dwg2::recomp_menu::HitRect {
        for (const auto& h : hits) {
          if (h.id == id && h.sub == sub) return h;
        }
        return {};
      };
      std::vector<dwg2::recomp_menu::HitRect> main_hits;
      auto main_snap = dwg2::recomp_menu::Snapshot(act);
      CHECK(main_snap.main_scroll == 0);
      dwg2::recomp_menu::DrawMenuLayout(main_snap, viewport.virt_w, "",
          [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {}, [](auto...) {},
          [&](auto box, int id, int sub) {
            main_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
          });
      CHECK(find_hit_in(main_hits, dwg2::recomp_menu::kHitMainScrollUp, 0).id ==
            dwg2::recomp_menu::kHitNone);
      const auto down_hit = find_hit_in(main_hits, dwg2::recomp_menu::kHitMainScrollDown, 0);
      CHECK(down_hit.id == dwg2::recomp_menu::kHitMainScrollDown);
      dwg2::recomp_menu::UpdateHitRects(main_hits, main_snap.view, main_snap.main_scroll, 0);
      CHECK(dwg2::recomp_menu::HandleMouseDown(
          (down_hit.x0 + down_hit.x1) / 2, (down_hit.y0 + down_hit.y1) / 2, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == 1);
      CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kFullscreen);

      // Scroll to MaxMainScroll() via mouse wheel, redraw, check Up arrow present and Down absent.
      for (int s = 0; s < dwg2::recomp_menu::MaxMainScroll(); ++s) {
        dwg2::recomp_menu::HandleMouseWheel(-1.0f);
      }
      main_snap = dwg2::recomp_menu::Snapshot(act);
      CHECK(main_snap.main_scroll == dwg2::recomp_menu::MaxMainScroll());
      main_hits.clear();
      dwg2::recomp_menu::DrawMenuLayout(main_snap, viewport.virt_w, "",
          [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {}, [](auto...) {},
          [&](auto box, int id, int sub) {
            main_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
          });
      CHECK(find_hit_in(main_hits, dwg2::recomp_menu::kHitMainScrollDown, 0).id ==
            dwg2::recomp_menu::kHitNone);
      const auto up_hit = find_hit_in(main_hits, dwg2::recomp_menu::kHitMainScrollUp, 0);
      CHECK(up_hit.id == dwg2::recomp_menu::kHitMainScrollUp);
      dwg2::recomp_menu::UpdateHitRects(main_hits, main_snap.view, main_snap.main_scroll, 0);

      // Button Prompts center click cycles forward (no reset to Auto),
      // Language center click is a no-op when unchanged and opens modal when changed.
      CHECK(find_hit_in(main_hits, dwg2::recomp_menu::kHitPromptChoice, 0).id ==
            dwg2::recomp_menu::kHitNone);
      const auto prompt_row =
          find_hit_in(main_hits, dwg2::recomp_menu::kHitMainRow, dwg2::recomp_menu::kButtonPrompts);
      const auto prompt_left = find_hit_in(main_hits, dwg2::recomp_menu::kHitPromptChoice, -1);
      const auto prompt_right = find_hit_in(main_hits, dwg2::recomp_menu::kHitPromptChoice, -2);
      CHECK(prompt_row.id == dwg2::recomp_menu::kHitMainRow &&
            prompt_left.id == dwg2::recomp_menu::kHitPromptChoice &&
            prompt_right.id == dwg2::recomp_menu::kHitPromptChoice);
      const float prompt_cx = (prompt_left.x1 + prompt_right.x0) / 2;
      const float prompt_cy = (prompt_row.y0 + prompt_row.y1) / 2;
      CHECK(m_prompts == 1);
      CHECK(dwg2::recomp_menu::HandleMouseDown(prompt_cx, prompt_cy, act, now));
      CHECK(m_prompts == 4);  // Cycled forward 1 -> 4, did not reset to 0

      // Clicking Up arrow steps main_scroll back by 1 (from MaxMainScroll() to MaxMainScroll()-1).
      CHECK(dwg2::recomp_menu::HandleMouseDown(
          (up_hit.x0 + up_hit.x1) / 2, (up_hit.y0 + up_hit.y1) / 2, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).main_scroll == dwg2::recomp_menu::MaxMainScroll() - 1);
      // Scroll back to bottom and refresh hits for Language clicks.
      dwg2::recomp_menu::HandleMouseWheel(-1.0f);
      main_snap = dwg2::recomp_menu::Snapshot(act);
      main_hits.clear();
      dwg2::recomp_menu::DrawMenuLayout(main_snap, viewport.virt_w, "",
          [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {}, [](auto...) {},
          [&](auto box, int id, int sub) {
            main_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
          });
      dwg2::recomp_menu::UpdateHitRects(main_hits, main_snap.view, main_snap.main_scroll, 0);

      const auto lang_left = find_hit_in(main_hits, dwg2::recomp_menu::kHitLanguageChoice, -1);
      const auto lang_center = find_hit_in(main_hits, dwg2::recomp_menu::kHitLanguageChoice, 0);
      const auto lang_right = find_hit_in(main_hits, dwg2::recomp_menu::kHitLanguageChoice, -2);
      CHECK(lang_left.id == dwg2::recomp_menu::kHitLanguageChoice &&
            lang_center.id == dwg2::recomp_menu::kHitLanguageChoice &&
            lang_right.id == dwg2::recomp_menu::kHitLanguageChoice);
      const float lc_x = (lang_center.x0 + lang_center.x1) / 2;
      const float lc_y = (lang_center.y0 + lang_center.y1) / 2;
      // Unchanged language center click stays in kMain.
      CHECK(dwg2::recomp_menu::HandleMouseDown(lc_x, lc_y, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
      // Cycle > (1->4), < (4->1), > (1->4), then center click -> opens modal.
      CHECK(dwg2::recomp_menu::HandleMouseDown((lang_right.x0 + lang_right.x1) / 2, lc_y, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == 4);
      CHECK(dwg2::recomp_menu::HandleMouseDown((lang_left.x0 + lang_left.x1) / 2, lc_y, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).pending_language == 1);
      CHECK(dwg2::recomp_menu::HandleMouseDown((lang_right.x0 + lang_right.x1) / 2, lc_y, act, now));
      CHECK(dwg2::recomp_menu::HandleMouseDown(lc_x, lc_y, act, now));
      CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kConfirmLanguageRestart);
    }
    modal_hits.clear();
    auto lang_modal = dwg2::recomp_menu::Snapshot(act);
    dwg2::recomp_menu::DrawMenuLayout(lang_modal, viewport.virt_w, "",
        [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {}, [](auto...) {},
        [&](auto box, int id, int sub) {
          modal_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
        });
    CHECK(modal_hits.size() == 2 && modal_hits.front().id == dwg2::recomp_menu::kHitQuitYes);
    dwg2::recomp_menu::UpdateHitRects(modal_hits, lang_modal.view, lang_modal.main_scroll, 0);
    const auto lang_yes = modal_hits.front();
    CHECK(dwg2::recomp_menu::HandleMouseDown(
        (lang_yes.x0 + lang_yes.x1) / 2, (lang_yes.y0 + lang_yes.y1) / 2, act, now));
    CHECK(m_lang == 4 && m_lang_restarts == 2);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);

    dwg2::recomp_menu::SetOpen(false);
    for (int i = 0; i < 3; ++i) dwg2::input::Set(i, saved_profiles[i]);
  }

  // Layout and hit bounds for small, 16:10, 4:3, 4K, ultrawide and portrait windows across all 5
  // languages and main scroll offsets (top, middle, bottom).
  {
    using namespace dwg2::recomp_menu;
    const auto pack = dwg2::ui_assets::detail::BuildFallbackPack();
    MenuSnapshot snap{};
    snap.open = true;
    snap.fov = 100;
    snap.resolution_pending = 3;
    snap.prompt_choice = 0;
    snap.prompt_style = dwg2::prompts::kXboxSeries; // Longest "Auto (Xbox Series)" label
    snap.version_text = std::string(64, 'W');
    snap.working_profiles[0] = dwg2::input::Defaults(0);
    CHECK(BindingLabel(snap.working_profiles[0].alternate[0], 0) == "Semicolon");
    CHECK(BindingLabel(snap.working_profiles[0].alternate[1], 0) == "Apostrophe");
    snap.working_profiles[0].bind[2] = {1, 8, 1, 7};
    snap.working_profiles[0].alternate[2] = {1, 192, 1, 7};
    constexpr std::array sizes{
        std::pair{640, 360}, std::pair{1024, 576}, std::pair{1280, 800},
        std::pair{1600, 1200}, std::pair{3840, 2160}, std::pair{3440, 1440},
        std::pair{3840, 1080}, std::pair{800, 1000},
    };
    for (int lang : kLanguageOrder) {
      const auto& t = l10n::Get(lang);
      snap.language = lang;
      snap.pending_language = lang;
      snap.toast = t.toast_save_err;
      for (auto [width, height] : sizes) {
        auto vp = ComputeViewport(float(width), float(height));
        for (const auto view : {View::kMain, View::kControlsWizard, View::kConfirmRestart,
                                View::kConfirmQuit, View::kConfirmLanguageRestart}) {
          snap.view = view;
          const int max_scroll = (view == View::kMain) ? MaxMainScroll() : 0;
          for (int sc = 0; sc <= max_scroll; ++sc) {
            snap.main_scroll = sc;
            const bool expect_about =
                (view != View::kMain) ||
                (LastVisibleMainRow(snap.main_scroll) == kMainRowCount - 1);
            std::vector<Box> text_slots;
            std::vector<HitRect> hits;
            size_t tri_count = 0;
            bool expected_title_visible = false;
            bool expected_blurb_visible = (view != View::kMain);
            bool expected_version_visible = (view != View::kMain);
            bool geometry_ok = true;
            const auto require = [&](bool condition) { geometry_ok &= condition; };
            const auto check_box = [&](Box box) {
              require(box.x0 >= 0 && box.y0 >= 0 && box.x1 <= vp.virt_w && box.y1 <= vp.virt_h);
              require(box.Width() > 0 && box.Height() > 0);
            };
            DrawMenuLayout(snap, vp.virt_w, "120 FPS / 120 SWAP", [&](Box box, uint32_t) {
              check_box(box);
              if (box.x0 == 0 && box.y0 == 0 && box.x1 == vp.virt_w && box.y1 == vp.virt_h) {
                text_slots.clear(); // The confirmation scrim starts a new visual layer.
              }
            }, [&](float x0, float y0, float x1, float y1, float x2, float y2, uint32_t) {
              for (float px : {x0, x1, x2}) require(px >= 0.0f && px <= vp.virt_w);
              for (float py : {y0, y1, y2}) {
                require(py >= 0.0f && py <= vp.virt_h);
                require(py < kMainViewTop || py > kMainViewBottom);
              }
              ++tri_count;
            }, [&](Box box, std::string_view label, float size, uint32_t, Align align) {
              check_box(box);
              if (view == View::kMain) {
                require(box.y1 <= 100.0f || box.y0 >= 624.0f ||
                        (box.y0 >= kMainViewTop && box.y1 <= kMainViewBottom));
              }
              if (label.empty()) return;
              float advance = 0;
              for (size_t i = 0; i < label.size();) {
                const uint32_t cp = dwg2::ui_assets::NextUtf8Codepoint(label, i);
                require(pack.HasGlyph(cp));
                advance += pack.GlyphForCodepoint(cp).advance;
              }
              const auto place = PlaceText(box, advance, size, align);
              const Box fitted{place.x, place.y, place.x + advance * place.scale,
                               place.y + 28.0f * place.scale};
              constexpr float epsilon = 0.001f;
              require(fitted.x0 >= box.x0 - epsilon && fitted.y0 >= box.y0 - epsilon);
              require(fitted.x1 <= box.x1 + epsilon && fitted.y1 <= box.y1 + epsilon);
              for (const auto& other : text_slots) {
                require(fitted.x1 <= other.x0 || other.x1 <= fitted.x0 ||
                        fitted.y1 <= other.y0 || other.y1 <= fitted.y0);
              }
              text_slots.push_back(fitted);
              expected_title_visible |=
                  (view == View::kMain && label == t.title_main) ||
                  (view == View::kControlsWizard && label == t.title_wizard) ||
                  (view == View::kConfirmRestart && label == t.restart_title) ||
                  (view == View::kConfirmQuit && label == t.quit_title) ||
                  (view == View::kConfirmLanguageRestart && label == t.lang_restart_title);
              if (view == View::kMain && label == t.made_by) expected_blurb_visible = true;
              if (view == View::kMain &&
                  label == l10n::FormatVersion(snap.language, snap.version_text)) {
                expected_version_visible = true;
              }
            }, [&](Box box, int id, int sub) {
              check_box(box);
              float x = (box.x0 + box.x1) / 2, y = (box.y0 + box.y1) / 2, vx = 0, vy = 0;
              require(ScreenToVirtual(vp.ox + x * vp.scale, vp.oy + y * vp.scale,
                                      float(width), float(height), vx, vy));
              require(std::abs(vx - x) < 0.001f && std::abs(vy - y) < 0.001f);
              hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
            });
            CHECK(geometry_ok);
            CHECK(expected_title_visible);
            CHECK(expected_blurb_visible == expect_about);
            CHECK(expected_version_visible == expect_about);
            if (view == View::kMain) {
              const size_t expected_tris =
                  ((snap.main_scroll > 0) ? 2u : 0u) +
                  ((LastVisibleMainRow(snap.main_scroll) < kMainRowCount - 1) ? 2u : 0u);
              CHECK(tri_count == expected_tris);
              if (snap.main_scroll == 0) {
                auto find_row = [&](int item) -> HitRect {
                  for (const auto& h : hits) {
                    if (h.id == kHitMainRow && h.sub == item) return h;
                  }
                  return {};
                };
                const auto p_hit = find_row(kPause);
                const auto r_hit = find_row(kRestart);
                const auto q_hit = find_row(kQuitGame);
                CHECK(p_hit.id == kHitMainRow && r_hit.id == kHitMainRow &&
                      q_hit.id == kHitMainRow);
                CHECK(p_hit.x0 < r_hit.x0 && r_hit.x0 < q_hit.x0);
              }
            }
            if (view == View::kConfirmRestart || view == View::kConfirmQuit ||
                view == View::kConfirmLanguageRestart) {
              CHECK(hits.size() == 2 && hits[0].id == kHitQuitYes && hits[1].id == kHitQuitNo);
            }
          }
        }
      }
    }
  }
  return 0;
}
