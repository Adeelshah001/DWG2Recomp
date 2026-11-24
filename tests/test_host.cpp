#include "automatic_save.h"
#include "camera_control.h"
#include "camera_refresh.h"
#include "game_ui_assets.h"
#include "input_config.h"
#include "keyboard_art.h"
#include "player_control_settings.h"
#include "prompt_control.h"
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
    CHECK(fb_missing.ascii_glyphs['0'].valid && fb_missing.ascii_glyphs['0'].cell_x == 1 * 28 &&
          fb_missing.ascii_glyphs['0'].cell_y == 2 * 28);
    CHECK(fb_missing.ascii_glyphs['A'].valid && fb_missing.ascii_glyphs['A'].cell_x == 11 * 28 &&
          fb_missing.ascii_glyphs['A'].cell_y == 2 * 28);
    CHECK(fb_missing.ascii_glyphs['Z'].valid && fb_missing.ascii_glyphs['Z'].cell_x == 0 &&
          fb_missing.ascii_glyphs['Z'].cell_y == 3 * 28);
    CHECK(fb_missing.ascii_glyphs['a'].valid && fb_missing.ascii_glyphs['a'].cell_x == 1 * 28 &&
          fb_missing.ascii_glyphs['a'].cell_y == 3 * 28);
    CHECK(fb_missing.ascii_glyphs['['].valid && fb_missing.ascii_glyphs['['].cell_x == 25 * 28 &&
          fb_missing.ascii_glyphs['['].cell_y == 0);
    CHECK(fb_missing.ascii_glyphs['%'].valid && fb_missing.ascii_glyphs['%'].cell_x == 16 * 28 &&
          fb_missing.ascii_glyphs['%'].cell_y == 1 * 28);
    CHECK(fb_missing.ascii_glyphs['A'].advance >= 6 && fb_missing.ascii_glyphs['A'].advance <= 28);

    const auto corrupt = std::filesystem::temp_directory_path() / L"dwg2_corrupt_linkdata_test.bns";
    {
      std::ofstream out(corrupt, std::ios::binary | std::ios::trunc);
      const char garbage[32] = "NOT_A_VALID_LINKDATA_FILE_00000";
      out.write(garbage, sizeof(garbage));
    }
    const auto fb_corrupt = dwg2::ui_assets::LoadPack(corrupt);
    std::filesystem::remove(corrupt, ec);
    CHECK(!fb_corrupt.from_game_data);
    CHECK(fb_corrupt.textures[dwg2::ui_assets::kTexFont].width == 1024);
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
    int m_res = 1, m_fov = 0, m_fps = 60, m_prompts = 0, m_save_calls = 0, m_restarts = 0;
    int m_update_checks = 0;

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
    act.set_turbo = [&](bool v) -> std::string { m_turbo = v; return {}; };
    act.set_prompts = [&](int p) -> std::string { m_prompts = p; return {}; };
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

    CHECK(dwg2::recomp_menu::kMainItemCount == 13);
    dwg2::recomp_menu::SetOpen(false);
    CHECK(!dwg2::recomp_menu::IsOpen());
    dwg2::recomp_menu::Toggle(false);
    CHECK(dwg2::recomp_menu::IsOpen());
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);

    uint64_t now = 100;
    auto kb_tap = [&](int vk) {
      dwg2::input::Raw r{};
      r.keys[vk] = true;
      dwg2::recomp_menu::HandleKeyboard(r, now, act);
      now += 20;
      dwg2::recomp_menu::HandleKeyboard(dwg2::input::Raw{}, now, act);
      now += 20;
    };

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
    kb_tap(40);
    kb_tap(13);
    CHECK(m_paused && dwg2::recomp_menu::Snapshot(act).paused);
    kb_tap(13);
    CHECK(!m_paused && dwg2::recomp_menu::IsOpen());
    kb_tap(38);
    kb_tap(38);
    kb_tap(13);
    CHECK(m_update_checks == 1 && dwg2::recomp_menu::Snapshot(act).toast == "Checking for updates...");
    kb_tap(40);

    // Walk all 13 items and verify wrap back to kRestart.
    for (int i = 0; i < dwg2::recomp_menu::kMainItemCount; ++i) {
      CHECK(dwg2::recomp_menu::Snapshot(act).focus == i);
      kb_tap(40);  // Down
    }
    CHECK(dwg2::recomp_menu::Snapshot(act).focus == dwg2::recomp_menu::kRestart);

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
    kb_tap(40);  // Turbo Mode
    kb_tap(13);
    CHECK(m_turbo && dwg2::recomp_menu::Snapshot(act).turbo);
    kb_tap(37);  // Left -> Off
    CHECK(!m_turbo);
    kb_tap(39);  // Right -> On
    CHECK(m_turbo);
    kb_tap(40);  // Button Prompts
    kb_tap(13);  // 0 -> 1
    CHECK(m_prompts == 1);

    // Inline Quit Yes/No (default No).
    while (dwg2::recomp_menu::Snapshot(act).focus != dwg2::recomp_menu::kQuitGame) kb_tap(38);
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

    // Re-enter wizard, adjust deadzone, restore defaults, save.
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

    dwg2::recomp_menu::UpdateHitRects({
        {50, 100, 90, 120, dwg2::recomp_menu::kHitWizBack, 0},
    }, dwg2::recomp_menu::View::kControlsWizard, dwg2::recomp_menu::Snapshot(act).wiz_scroll);
    CHECK(dwg2::recomp_menu::HandleMouseDown(70.0f, 110.0f, act, now));
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    dwg2::recomp_menu::UpdateHitRects({
        {100, 100, 300, 120, dwg2::recomp_menu::kHitFovSlider, 0},
        {320, 100, 360, 120, dwg2::recomp_menu::kHitResStop, 1},
        {400, 100, 440, 120, dwg2::recomp_menu::kHitClose, 0},
        {460, 100, 500, 120, dwg2::recomp_menu::kHitToggleOn, dwg2::recomp_menu::kFullscreen},
        {520, 100, 560, 120, dwg2::recomp_menu::kHitToggleOff, dwg2::recomp_menu::kFullscreen},
        {650, 100, 690, 120, dwg2::recomp_menu::kHitToggleOn, dwg2::recomp_menu::kTurbo},
        {700, 100, 740, 120, dwg2::recomp_menu::kHitToggleOff, dwg2::recomp_menu::kTurbo},
        {580, 100, 640, 120, dwg2::recomp_menu::kHitMainRow, dwg2::recomp_menu::kRestart},
    }, dwg2::recomp_menu::View::kMain, 0);
    CHECK(dwg2::recomp_menu::HandleMouseDown(200.0f, 110.0f, act, now));
    CHECK(m_fov == 50);
    dwg2::recomp_menu::HandleMouseMove(250.0f, 110.0f, act);
    CHECK(m_fov == 75);
    dwg2::recomp_menu::HandleMouseUp();
    CHECK(dwg2::recomp_menu::HandleMouseDown(340.0f, 110.0f, act, now));
    CHECK(m_res == 1);
    dwg2::recomp_menu::HandleMouseWheel(-1.0f);  // in kMain -> no-op
    CHECK(dwg2::recomp_menu::HandleMouseDown(480.0f, 110.0f, act, now) && m_full);
    CHECK(dwg2::recomp_menu::HandleMouseDown(480.0f, 110.0f, act, now) && m_full);
    CHECK(dwg2::recomp_menu::HandleMouseDown(540.0f, 110.0f, act, now) && !m_full);
    CHECK(dwg2::recomp_menu::HandleMouseDown(670.0f, 110.0f, act, now) && m_turbo);
    CHECK(dwg2::recomp_menu::HandleMouseDown(720.0f, 110.0f, act, now) && !m_turbo);
    CHECK(dwg2::recomp_menu::HandleMouseDown(600.0f, 110.0f, act, now));
    CHECK(!dwg2::recomp_menu::HandleMouseDown(420.0f, 110.0f, act, now)); // Stale main hit.

    std::vector<dwg2::recomp_menu::HitRect> modal_hits;
    restart = dwg2::recomp_menu::Snapshot(act);
    const auto viewport = dwg2::recomp_menu::ComputeViewport(1280, 800);
    dwg2::recomp_menu::DrawMenuLayout(restart, viewport.virt_w, "",
        [](dwg2::recomp_menu::Box, uint32_t) {}, [](auto...) {},
        [&](auto box, int id, int sub) {
          modal_hits.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
        });
    CHECK(modal_hits.size() == 2 && modal_hits.front().id == dwg2::recomp_menu::kHitQuitYes);
    dwg2::recomp_menu::UpdateHitRects(modal_hits, restart.view, 0);
    const auto yes = modal_hits.front();
    CHECK(dwg2::recomp_menu::ScreenToVirtual(
        viewport.ox + (yes.x0 + yes.x1) / 2 * viewport.scale,
        viewport.oy + (yes.y0 + yes.y1) / 2 * viewport.scale, 1280, 800, vx, vy));
    CHECK(dwg2::recomp_menu::HandleMouseDown(vx, vy, act, now) && m_restarts == 2);
    CHECK(dwg2::recomp_menu::Snapshot(act).view == dwg2::recomp_menu::View::kMain);
    dwg2::recomp_menu::SetOpen(false);
    for (int i = 0; i < 3; ++i) dwg2::input::Set(i, saved_profiles[i]);
  }

  // Layout and hit bounds for small, 16:10, 4:3, 4K, ultrawide and portrait windows.
  {
    using namespace dwg2::recomp_menu;
    const auto font = dwg2::ui_assets::detail::BuildFallbackPack().ascii_glyphs;
    MenuSnapshot snap{};
    snap.open = true;
    snap.fov = 100;
    snap.resolution_pending = 3;
    snap.toast = "Could not save settings.ini. The previous settings are retained.";
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
    for (auto [width, height] : sizes) {
      auto vp = ComputeViewport(float(width), float(height));
      for (const auto view : {View::kMain, View::kControlsWizard, View::kConfirmRestart, View::kConfirmQuit}) {
        snap.view = view;
        std::vector<Box> text_slots;
        std::vector<HitRect> hits;
        bool expected_title_visible = false;
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
        }, [&](Box box, std::string_view label, float size, uint32_t, Align align) {
          check_box(box);
          if (label.empty()) return;
          float advance = 0;
          for (unsigned char ch : label) {
            require(ch < font.size() && font[ch].valid);
            if (ch < font.size()) advance += font[ch].advance;
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
              (view == View::kMain && label == "Recomp Menu") ||
              (view == View::kControlsWizard && label == "Controls Configuration") ||
              (view == View::kConfirmRestart && label == "Restart Game?") ||
              (view == View::kConfirmQuit && label == "Quit Game?");
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
        if (view == View::kConfirmRestart || view == View::kConfirmQuit) {
          CHECK(hits.size() == 2 && hits[0].id == kHitQuitYes && hits[1].id == kHitQuitNo);
        }
      }
    }
  }
  return 0;
}
