// Original host Recomp Menu state, navigation, hit-testing, and control capture.
// Header-only and D3D12/SDL-free so test_host.cpp can unit-test all flows directly.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "input_config.h"
#include "player_control_settings.h"
#include "prompt_control.h"
#include "recomp_menu_l10n.h"
#include "video_control.h"

namespace dwg2::recomp_menu {

enum class View { kMain, kConfirmQuit, kConfirmRestart, kConfirmLanguageRestart, kControlsWizard };

enum MainItem : int {
  kPause = 0,
  kRestart,
  kQuitGame,
  kFullscreen,
  kResolution,
  kFov,
  kFps,
  kFpsCounter,
  kVsync,
  kTurbo,
  kButtonPrompts,
  kConfigureControls,
  kLanguage,
  kCheckUpdates,
  kMainItemCount
};

enum HitId : int {
  kHitNone = 0,
  kHitMainRow,
  kHitMainScrollUp,
  kHitMainScrollDown,
  kHitResStop,
  kHitFovSlider,
  kHitFpsChoice,
  kHitPromptChoice,
  kHitLanguageChoice,
  kHitToggleOff,
  kHitToggleOn,
  kHitQuitYes,
  kHitQuitNo,
  kHitWizPrevProfile,
  kHitWizNextProfile,
  kHitWizProfileTab,
  kHitWizRow,
  kHitWizDeadzone,
  kHitWizDefaults,
  kHitWizSave,
  kHitWizBack,
  kHitClose
};

inline constexpr int kMainRowCount = 12;
inline constexpr float kMainViewTop = 116.0f;
inline constexpr float kMainViewBottom = 598.0f;

constexpr float MainRowHeight(int row) {
  switch (row) {
    case 0: return 42.0f;
    case 1:
    case 5:
    case 6:
    case 7: return 38.0f;
    case 2:
    case 8:
    case 9:
    case 10: return 39.0f;
    case 3:
    case 11: return 52.0f;
    case 4: return 67.0f;
    default: return 0.0f;
  }
}

constexpr float MainRowGap(int row) {
  switch (row) {
    case 0:
    case 3:
    case 7:
    case 9:
    case 10: return 12.0f;
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
    case 8: return 7.0f;
    default: return 0.0f;
  }
}

constexpr int FocusToMainRow(int focus) {
  if (focus <= kQuitGame) return 0;
  return std::clamp(focus - kQuitGame, 1, kMainRowCount - 1);
}

constexpr int MainRowToFocus(int row) {
  const int r = std::clamp(row, 0, kMainRowCount - 1);
  return (r == 0) ? kPause : (kQuitGame + r);
}

constexpr int LastVisibleMainRow(int scroll) {
  const int start = std::clamp(scroll, 0, kMainRowCount - 1);
  float y = kMainViewTop;
  int last = start;
  for (int r = start; r < kMainRowCount; ++r) {
    const float h = MainRowHeight(r);
    if (r > start && y + h > kMainViewBottom) break;
    last = r;
    y += h + MainRowGap(r);
  }
  return last;
}

constexpr int MaxMainScroll() {
  for (int s = 0; s < kMainRowCount; ++s) {
    if (LastVisibleMainRow(s) >= kMainRowCount - 1) return s;
  }
  return kMainRowCount - 1;
}

inline constexpr int kWizVisibleRows = 9;
inline constexpr float kSliderThumbRadius = 5.0f;
inline constexpr int kWizDeadzone = dwg2::input::Count;
inline constexpr int kWizDefaults = dwg2::input::Count + 1;
inline constexpr int kWizSave = dwg2::input::Count + 2;
inline constexpr int kWizBack = dwg2::input::Count + 3;
inline constexpr int kWizItemCount = dwg2::input::Count + 4;
inline constexpr uint64_t kRepeatDelayMs = 320;
inline constexpr uint64_t kRepeatRateMs = 95;
inline constexpr uint64_t kStickSnapbackMs = 150;
inline constexpr uint64_t kCrossSourceConfirmDebounceMs = 80;
inline constexpr uint64_t kToastDurationMs = 3000;

struct HitRect {
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int id = kHitNone;
  int sub = 0;
};

struct Actions {
  std::function<std::string()> restart;
  std::function<std::string()> toggle_pause;
  std::function<void()> quit_game;
  std::function<std::string(bool)> set_fullscreen;
  std::function<std::string(int)> set_resolution;
  std::function<std::string(int)> set_fov;
  std::function<std::string(int)> set_fps;
  std::function<std::string(bool)> set_fps_counter;
  std::function<std::string(bool)> set_vsync;
  std::function<std::string(bool)> set_turbo;
  std::function<std::string(int)> set_prompts;
  std::function<std::string(int)> apply_language;
  std::function<std::string()> check_updates;
  std::function<std::string(const std::array<dwg2::input::Profile, 3>&)> save_controls;

  std::function<bool()> is_paused;
  std::function<bool()> is_fullscreen;
  std::function<int()> resolution;
  std::function<int()> resolution_pending;
  std::function<int()> fov;
  std::function<int()> fps;
  std::function<bool()> fps_counter;
  std::function<bool()> vsync;
  std::function<bool()> turbo;
  std::function<int()> prompt_choice;
  std::function<int()> prompt_style;
  std::function<int()> language;
  std::function<std::string()> version_text;
};

struct MenuSnapshot {
  bool open = false;
  View view = View::kMain;
  int focus = 0;
  int main_scroll = 0;
  bool confirm_quit_yes = false;

  int wiz_profile = 0;
  int wiz_cursor = 0;
  int wiz_scroll = 0;
  int capturing_action = -1;
  std::array<dwg2::input::Profile, 3> working_profiles{};

  std::string toast;
  uint64_t toast_expiry_ms = 0;

  bool paused = false;
  bool fullscreen = false;
  int resolution = 1;
  int resolution_pending = 0;
  int fov = 0;
  int fps = 60;
  bool fps_counter = false;
  bool vsync = false;
  bool turbo = false;
  int prompt_choice = 0;
  int prompt_style = 1;
  int language = kLangEnglish;
  int pending_language = kLangEnglish;
  std::string version_text;
};

inline std::atomic<bool> open{false};
inline std::atomic<bool> capturing{false};

inline bool IsOpen() { return open.load(std::memory_order_acquire); }
inline bool IsCapturing() { return capturing.load(std::memory_order_acquire); }

namespace detail {

struct RepeatState {
  int dir = 0;
  uint64_t next_ms = 0;
  int last_stick_dir = 0;
  uint64_t stick_release_ms = 0;
  int Step(int cur_dir, uint64_t now_ms) {
    if (cur_dir == 0) {
      dir = 0;
      next_ms = 0;
      return 0;
    }
    if (cur_dir != dir) {
      dir = cur_dir;
      next_ms = now_ms + kRepeatDelayMs;
      return dir;
    }
    if (now_ms >= next_ms) {
      next_ms = now_ms + kRepeatRateMs;
      return dir;
    }
    return 0;
  }
  int StepGamepad(int dpad_dir, int stick_dir, uint64_t now_ms) {
    if (dpad_dir != 0) {
      last_stick_dir = 0;
      stick_release_ms = 0;
      return Step(dpad_dir, now_ms);
    }
    if (stick_dir != 0) {
      if (last_stick_dir != 0 && stick_dir == -last_stick_dir &&
          now_ms < stick_release_ms + kStickSnapbackMs) {
        return Step(0, now_ms);
      }
      last_stick_dir = stick_dir;
      stick_release_ms = 0;
      return Step(stick_dir, now_ms);
    }
    if (dir != 0 && last_stick_dir == dir) stick_release_ms = now_ms;
    return Step(0, now_ms);
  }
  void Reset() {
    dir = 0;
    next_ms = 0;
    last_stick_dir = 0;
    stick_release_ms = 0;
  }
};

struct State {
  std::mutex mutex;
  View view = View::kMain;
  int focus = kPause;
  int main_scroll = 0;
  bool confirm_quit_yes = false;
  int active_language = kLangEnglish;
  int pending_language = kLangEnglish;
  bool pending_language_dirty = false;

  int wiz_profile = 0;
  int wiz_cursor = 0;
  int wiz_scroll = 0;
  int capturing_action = -1;
  std::array<dwg2::input::Profile, 3> working_profiles{};
  std::array<bool, 256> blocked_keys{};
  dwg2::input::Raw blocked_gp{};
  int modifier_candidate = 0;
  int modifier_bits = 0;
  dwg2::input::Raw last_kb{};
  dwg2::input::Raw last_gp{};

  std::string toast;
  uint64_t toast_expiry_ms = 0;

  std::vector<HitRect> hit_rects;
  View hit_view = View::kMain;
  int hit_main_scroll = 0;
  int hit_wiz_scroll = 0;
  int dragging_hit = kHitNone;
  HitRect drag_rect{};

  RepeatState gp_v{}, gp_h{}, kb_v{}, kb_h{};
  bool prev_a = false, prev_b = false, prev_x = false, prev_lb = false, prev_rb = false;
  bool prev_gp_close = false, gp_close_wait_release = false;
  bool prev_enter = false, prev_esc = false, prev_back = false, prev_del = false;
  bool prev_kb_close = false, kb_close_wait_release = false;
  int last_confirm_source = 0;
  uint64_t last_confirm_ms = 0;
};

inline State& GetState() {
  static State s;
  return s;
}

inline void SetToastLocked(State& s, const std::string& msg, uint64_t now_ms) {
  if (msg.empty()) return;
  s.toast = msg;
  s.toast_expiry_ms = now_ms > 0 ? (now_ms + kToastDurationMs) : 0;
}

inline void EnsureMainScrollLocked(State& s) {
  const int max_scroll = MaxMainScroll();
  const int focus_row = FocusToMainRow(s.focus);
  if (focus_row < s.main_scroll) s.main_scroll = focus_row;
  while (focus_row > LastVisibleMainRow(s.main_scroll) && s.main_scroll < max_scroll) {
    ++s.main_scroll;
  }
  s.main_scroll = std::clamp(s.main_scroll, 0, max_scroll);
}

inline void ClampMainFocusToViewLocked(State& s) {
  const int first = std::clamp(s.main_scroll, 0, MaxMainScroll());
  const int last = LastVisibleMainRow(first);
  const int focus_row = FocusToMainRow(s.focus);
  if (focus_row < first) s.focus = MainRowToFocus(first);
  else if (focus_row > last) s.focus = MainRowToFocus(last);
}

inline void EnsureWizScrollLocked(State& s) {
  const int max_scroll = std::max(0, dwg2::input::Count - kWizVisibleRows);
  if (s.wiz_cursor < dwg2::input::Count) {
    if (s.wiz_cursor < s.wiz_scroll) s.wiz_scroll = s.wiz_cursor;
    if (s.wiz_cursor >= s.wiz_scroll + kWizVisibleRows) {
      s.wiz_scroll = s.wiz_cursor - kWizVisibleRows + 1;
    }
  }
  s.wiz_scroll = std::clamp(s.wiz_scroll, 0, max_scroll);
}

inline void SyncLanguageLocked(State& s, const Actions& act) {
  if (act.language) s.active_language = NormalizeLanguage(act.language());
  if (!s.pending_language_dirty) s.pending_language = s.active_language;
}

inline void SetOpenLocked(State& s, bool value, bool wait_close_release) {
  const bool was = open.load(std::memory_order_acquire);
  open.store(value, std::memory_order_release);
  if (value && !was) {
    s.hit_rects.clear();
    s.view = View::kMain;
    s.focus = kPause;
    s.main_scroll = 0;
    s.hit_main_scroll = 0;
    s.confirm_quit_yes = false;
    s.pending_language_dirty = false;
    s.pending_language = s.active_language;
    s.capturing_action = -1;
    capturing.store(false, std::memory_order_release);
    s.toast.clear();
    s.toast_expiry_ms = 0;
    s.dragging_hit = kHitNone;
    s.gp_v.Reset();
    s.gp_h.Reset();
    s.kb_v.Reset();
    s.kb_h.Reset();
    s.prev_a = s.prev_b = s.prev_x = s.prev_lb = s.prev_rb = false;
    s.prev_enter = s.prev_esc = s.prev_back = s.prev_del = false;
    s.prev_gp_close = s.prev_kb_close = false;
    s.gp_close_wait_release = wait_close_release;
    s.kb_close_wait_release = wait_close_release;
    s.last_confirm_source = 0;
    s.last_confirm_ms = 0;
  } else if (!value) {
    s.pending_language_dirty = false;
    s.pending_language = s.active_language;
    s.capturing_action = -1;
    capturing.store(false, std::memory_order_release);
    s.dragging_hit = kHitNone;
    s.gp_close_wait_release = false;
    s.kb_close_wait_release = false;
    s.prev_gp_close = false;
    s.prev_kb_close = false;
    s.last_confirm_source = 0;
    s.last_confirm_ms = 0;
    s.hit_rects.clear();
  }
}

inline void EnterWizardLocked(State& s, int preferred_profile) {
  for (int i = 0; i < 3; ++i) s.working_profiles[i] = dwg2::input::Get(i);
  s.wiz_profile = std::clamp(preferred_profile, 0, 2);
  s.wiz_cursor = 0;
  s.wiz_scroll = 0;
  s.capturing_action = -1;
  capturing.store(false, std::memory_order_release);
  s.view = View::kControlsWizard;
}

inline void BeginCaptureLocked(State& s, int action_idx, uint64_t now_ms) {
  s.capturing_action = std::clamp(action_idx, 0, dwg2::input::Count - 1);
  capturing.store(true, std::memory_order_release);
  s.blocked_keys = s.last_kb.keys;
  s.blocked_gp = s.last_gp;
  s.modifier_candidate = 0;
  s.modifier_bits = 0;
  const auto& t = l10n::Get(s.active_language);
  SetToastLocked(s, s.wiz_profile == 0 ? t.toast_capture_kb : t.toast_capture_gp, now_ms);
}

inline bool TrySaveWizard(State& s, const Actions& act, uint64_t now_ms) {
  std::array<dwg2::input::Profile, 3> copy;
  int lang = kLangEnglish;
  {
    std::lock_guard lock(s.mutex);
    copy = s.working_profiles;
    lang = s.active_language;
  }
  const auto& t = l10n::Get(lang);
  std::string err;
  bool ok = true;
  if (act.save_controls) {
    err = act.save_controls(copy);
    ok = err.empty();
  } else {
    ok = dwg2::input::SavePlayerControls(copy);
    if (!ok) err = t.toast_save_err;
  }
  if (ok) {
    for (int i = 0; i < 3; ++i) dwg2::input::Set(i, copy[i]);
  }
  std::lock_guard lock(s.mutex);
  SetToastLocked(s, ok ? t.toast_saved_controls : err, now_ms);
  return ok;
}

inline int StepChoice(const int* opts, int count, int cur, int delta, bool wrap) {
  int idx = 0;
  for (int i = 0; i < count; ++i) {
    if (opts[i] == cur) {
      idx = i;
      break;
    }
  }
  if (wrap) return opts[(idx + delta + count) % count];
  return opts[std::clamp(idx + delta, 0, count - 1)];
}

enum class PendingOp {
  kNone,
  kRestart,
  kTogglePause,
  kQuitGame,
  kSetFullscreen,
  kSetResolution,
  kSetFov,
  kSetFps,
  kSetFpsCounter,
  kSetVsync,
  kSetTurbo,
  kSetPrompts,
  kApplyLanguageAndRestart,
  kCheckUpdates,
  kSaveWizard
};

struct Command {
  PendingOp op = PendingOp::kNone;
  bool bval = false;
  int ival = 0;
};

inline void ExecuteCommand(const Command& cmd, const Actions& act, uint64_t now_ms) {
  std::string toast;
  switch (cmd.op) {
    case PendingOp::kRestart:
      if (act.restart) toast = act.restart();
      break;
    case PendingOp::kTogglePause:
      if (act.toggle_pause) toast = act.toggle_pause();
      break;
    case PendingOp::kQuitGame:
      if (act.quit_game) act.quit_game();
      return;
    case PendingOp::kSetFullscreen:
      if (act.set_fullscreen) toast = act.set_fullscreen(cmd.bval);
      break;
    case PendingOp::kSetResolution:
      if (act.set_resolution) toast = act.set_resolution(cmd.ival);
      break;
    case PendingOp::kSetFov:
      if (act.set_fov) toast = act.set_fov(cmd.ival);
      break;
    case PendingOp::kSetFps:
      if (act.set_fps) toast = act.set_fps(cmd.ival);
      break;
    case PendingOp::kSetFpsCounter:
      if (act.set_fps_counter) toast = act.set_fps_counter(cmd.bval);
      break;
    case PendingOp::kSetVsync:
      if (act.set_vsync) toast = act.set_vsync(cmd.bval);
      break;
    case PendingOp::kSetTurbo:
      if (act.set_turbo) toast = act.set_turbo(cmd.bval);
      break;
    case PendingOp::kSetPrompts:
      if (act.set_prompts) toast = act.set_prompts(cmd.ival);
      break;
    case PendingOp::kApplyLanguageAndRestart:
      if (act.apply_language) toast = act.apply_language(cmd.ival);
      break;
    case PendingOp::kCheckUpdates:
      if (act.check_updates) toast = act.check_updates();
      break;
    case PendingOp::kSaveWizard:
      TrySaveWizard(GetState(), act, now_ms);
      return;
    case PendingOp::kNone:
      return;
  }
  if (!toast.empty()) {
    auto& s = GetState();
    std::lock_guard lock(s.mutex);
    SetToastLocked(s, toast, now_ms);
  }
}

inline Command BuildMainActivateLocked(State& s, int item, const Actions& act, int preferred_profile) {
  static constexpr int kFpsOpts[3] = {60, 120, 0};
  switch (item) {
    case kRestart:
      s.view = View::kConfirmRestart;
      s.confirm_quit_yes = false;
      return {};
    case kPause:
      return {PendingOp::kTogglePause};
    case kQuitGame:
      s.view = View::kConfirmQuit;
      s.confirm_quit_yes = false;
      return {};
    case kFullscreen:
      return {PendingOp::kSetFullscreen, !(act.is_fullscreen && act.is_fullscreen()), 0};
    case kResolution: {
      int cur = act.resolution ? act.resolution() : 1;
      int next = (cur >= 1 && cur <= 3) ? (cur % 3) + 1 : 1;
      return {PendingOp::kSetResolution, false, next};
    }
    case kFov: {
      int cur = act.fov ? act.fov() : 0;
      int next = (cur + 5 > 100) ? 0 : ((cur / 5) * 5 + 5);
      return {PendingOp::kSetFov, false, next};
    }
    case kFps: {
      int cur = act.fps ? act.fps() : 60;
      return {PendingOp::kSetFps, false, StepChoice(kFpsOpts, 3, cur, 1, true)};
    }
    case kFpsCounter:
      return {PendingOp::kSetFpsCounter, !(act.fps_counter && act.fps_counter()), 0};
    case kVsync:
      return {PendingOp::kSetVsync, !(act.vsync && act.vsync()), 0};
    case kTurbo:
      return {PendingOp::kSetTurbo, !(act.turbo && act.turbo()), 0};
    case kButtonPrompts: {
      int cur = act.prompt_choice ? act.prompt_choice() : 0;
      return {PendingOp::kSetPrompts, false,
              StepChoice(dwg2::prompts::kMenuOrder, 9, cur, 1, true)};
    }
    case kConfigureControls:
      EnterWizardLocked(s, preferred_profile);
      return {};
    case kLanguage: {
      const int target = s.pending_language_dirty ? s.pending_language : s.active_language;
      if (target == s.active_language) {
        s.pending_language_dirty = false;
        s.pending_language = s.active_language;
        return {};
      }
      s.pending_language = target;
      s.view = View::kConfirmLanguageRestart;
      s.confirm_quit_yes = false;
      return {};
    }
    case kCheckUpdates:
      return {PendingOp::kCheckUpdates};
    default:
      return {};
  }
}

inline Command BuildMainHorizLocked(State& s, int item, int dir, const Actions& act) {
  static constexpr int kFpsOpts[3] = {60, 120, 0};
  switch (item) {
    case kFullscreen: {
      const bool next = dir > 0;
      const bool cur = act.is_fullscreen && act.is_fullscreen();
      return next != cur ? Command{PendingOp::kSetFullscreen, next, 0} : Command{};
    }
    case kResolution: {
      int cur = act.resolution ? act.resolution() : 1;
      if (cur == 15) cur = (dir < 0) ? 2 : 1;
      int next = std::clamp(cur + dir, 1, 3);
      return next != cur ? Command{PendingOp::kSetResolution, false, next} : Command{};
    }
    case kFov: {
      int cur = act.fov ? act.fov() : 0;
      int snapped = std::clamp(((cur + 2) / 5) * 5, 0, 100);
      int next = std::clamp(snapped + dir * 5, 0, 100);
      return next != cur ? Command{PendingOp::kSetFov, false, next} : Command{};
    }
    case kFps: {
      int cur = act.fps ? act.fps() : 60;
      int next = StepChoice(kFpsOpts, 3, cur, dir, false);
      return next != cur ? Command{PendingOp::kSetFps, false, next} : Command{};
    }
    case kFpsCounter: {
      const bool next = dir > 0;
      const bool cur = act.fps_counter && act.fps_counter();
      return next != cur ? Command{PendingOp::kSetFpsCounter, next, 0} : Command{};
    }
    case kVsync: {
      const bool next = dir > 0;
      const bool cur = act.vsync && act.vsync();
      return next != cur ? Command{PendingOp::kSetVsync, next, 0} : Command{};
    }
    case kTurbo: {
      const bool next = dir > 0;
      const bool cur = act.turbo && act.turbo();
      return next != cur ? Command{PendingOp::kSetTurbo, next, 0} : Command{};
    }
    case kButtonPrompts: {
      int cur = act.prompt_choice ? act.prompt_choice() : 0;
      int next = StepChoice(dwg2::prompts::kMenuOrder, 9, cur, dir, true);
      return next != cur ? Command{PendingOp::kSetPrompts, false, next} : Command{};
    }
    case kLanguage: {
      const int cur = s.pending_language_dirty ? s.pending_language : s.active_language;
      s.pending_language = StepChoice(kLanguageOrder, 5, cur, dir, true);
      s.pending_language_dirty = true;
      return {};
    }
    default:
      return {};
  }
}

inline Command NavigateLocked(State& s, int v_step, int h_step, bool confirm, bool back,
                              bool clear_wiz, int wiz_prof_step, const Actions& act,
                              uint64_t now_ms, int preferred_profile) {
  if (s.view == View::kMain) {
    if (back) {
      SetOpenLocked(s, false, false);
      return {};
    }
    if (v_step != 0) {
      if (s.focus <= kQuitGame) {
        s.focus = (v_step > 0) ? kFullscreen : (kMainItemCount - 1);
      } else if (v_step < 0 && s.focus == kFullscreen) {
        s.focus = kPause;
      } else if (v_step > 0 && s.focus == kMainItemCount - 1) {
        s.focus = kPause;
      } else {
        s.focus += v_step;
      }
      EnsureMainScrollLocked(s);
    }
    if (h_step != 0) {
      if (s.focus <= kQuitGame) {
        s.focus = kPause + ((s.focus - kPause + h_step + 3) % 3);
        EnsureMainScrollLocked(s);
        return {};
      }
      EnsureMainScrollLocked(s);
      return BuildMainHorizLocked(s, s.focus, h_step, act);
    }
    if (confirm) {
      EnsureMainScrollLocked(s);
      return BuildMainActivateLocked(s, s.focus, act, preferred_profile);
    }
    return {};
  }

  if (s.view == View::kConfirmQuit || s.view == View::kConfirmRestart ||
      s.view == View::kConfirmLanguageRestart) {
    const bool is_lang_restart = (s.view == View::kConfirmLanguageRestart);
    const bool is_restart = (s.view == View::kConfirmRestart);
    const int return_focus = is_lang_restart ? kLanguage : (is_restart ? kRestart : kQuitGame);
    if (back) {
      s.view = View::kMain;
      s.focus = return_focus;
      EnsureMainScrollLocked(s);
      if (is_lang_restart) {
        s.pending_language_dirty = false;
        s.pending_language = s.active_language;
      }
      return {};
    }
    if (h_step < 0) s.confirm_quit_yes = true;
    else if (h_step > 0) s.confirm_quit_yes = false;
    else if (v_step != 0) s.confirm_quit_yes = !s.confirm_quit_yes;
    if (confirm) {
      if (s.confirm_quit_yes) {
        if (is_lang_restart) {
          const int lang = NormalizeLanguage(s.pending_language);
          s.view = View::kMain;
          s.focus = kLanguage;
          EnsureMainScrollLocked(s);
          s.pending_language_dirty = false;
          return {PendingOp::kApplyLanguageAndRestart, false, lang};
        }
        if (is_restart) {
          s.view = View::kMain;
          s.focus = kRestart;
          EnsureMainScrollLocked(s);
          return {PendingOp::kRestart};
        }
        SetOpenLocked(s, false, false);
        return {PendingOp::kQuitGame};
      }
      s.view = View::kMain;
      s.focus = return_focus;
      EnsureMainScrollLocked(s);
      if (is_lang_restart) {
        s.pending_language_dirty = false;
        s.pending_language = s.active_language;
      }
    }
    return {};
  }

  // View::kControlsWizard (not capturing)
  if (back) {
    s.view = View::kMain;
    s.focus = kConfigureControls;
    EnsureMainScrollLocked(s);
    return {};
  }
  if (wiz_prof_step != 0) {
    s.wiz_profile = (s.wiz_profile + wiz_prof_step + 3) % 3;
  }
  if (v_step != 0) {
    s.wiz_cursor = (s.wiz_cursor + v_step + kWizItemCount) % kWizItemCount;
    EnsureWizScrollLocked(s);
  }
  if (h_step != 0) {
    if (s.wiz_cursor == kWizDeadzone) {
      int& dz = s.working_profiles[s.wiz_profile].deadzone;
      dz = std::clamp(dz + h_step * 5, 0, 50);
    } else if (s.wiz_cursor >= kWizDefaults && s.wiz_cursor <= kWizBack) {
      int idx = s.wiz_cursor - kWizDefaults;
      s.wiz_cursor = kWizDefaults + ((idx + h_step + 3) % 3);
    } else {
      s.wiz_profile = (s.wiz_profile + h_step + 3) % 3;
    }
  }
  if (clear_wiz && s.wiz_cursor >= 0 && s.wiz_cursor < dwg2::input::Count) {
    s.working_profiles[s.wiz_profile].bind[s.wiz_cursor] = {};
    s.working_profiles[s.wiz_profile].alternate[s.wiz_cursor] = {};
    SetToastLocked(s, l10n::Get(s.active_language).toast_cleared, now_ms);
  }
  if (confirm) {
    if (s.wiz_cursor >= 0 && s.wiz_cursor < dwg2::input::Count) {
      BeginCaptureLocked(s, s.wiz_cursor, now_ms);
    } else if (s.wiz_cursor == kWizDefaults) {
      s.working_profiles[s.wiz_profile] = dwg2::input::Defaults(s.wiz_profile);
      SetToastLocked(s, l10n::Get(s.active_language).toast_defaults, now_ms);
    } else if (s.wiz_cursor == kWizSave) {
      return {PendingOp::kSaveWizard};
    } else if (s.wiz_cursor == kWizBack) {
      s.view = View::kMain;
      s.focus = kConfigureControls;
      EnsureMainScrollLocked(s);
    }
  }
  return {};
}

}  // namespace detail

inline void SetOpen(bool value, bool wait_close_release = false) {
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  detail::SetOpenLocked(s, value, wait_close_release);
}

inline void Toggle(bool wait_close_release = false) {
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  detail::SetOpenLocked(s, !open.load(std::memory_order_acquire), wait_close_release);
}

inline MenuSnapshot Snapshot(const Actions& act = {}, uint64_t now_ms = 0) {
  MenuSnapshot snap;
  auto& s = detail::GetState();
  {
    std::lock_guard lock(s.mutex);
    detail::SyncLanguageLocked(s, act);
    snap.open = open.load(std::memory_order_acquire);
    snap.view = s.view;
    snap.focus = s.focus;
    snap.main_scroll = s.main_scroll;
    snap.confirm_quit_yes = s.confirm_quit_yes;
    snap.language = s.active_language;
    snap.pending_language = s.pending_language_dirty ? s.pending_language : s.active_language;
    snap.wiz_profile = s.wiz_profile;
    snap.wiz_cursor = s.wiz_cursor;
    snap.wiz_scroll = s.wiz_scroll;
    snap.capturing_action = s.capturing_action;
    snap.working_profiles = s.working_profiles;
    if (!s.toast.empty() && (now_ms == 0 || s.toast_expiry_ms == 0 || now_ms < s.toast_expiry_ms)) {
      snap.toast = s.toast;
      snap.toast_expiry_ms = s.toast_expiry_ms;
    }
  }
  snap.paused = act.is_paused ? act.is_paused() : false;
  snap.fullscreen = act.is_fullscreen ? act.is_fullscreen() : false;
  snap.resolution = act.resolution ? act.resolution() : 1;
  snap.resolution_pending = act.resolution_pending ? act.resolution_pending() : 0;
  snap.fov = act.fov ? act.fov() : 0;
  snap.fps = act.fps ? act.fps() : 60;
  snap.fps_counter = act.fps_counter ? act.fps_counter() : false;
  snap.vsync = act.vsync ? act.vsync() : false;
  snap.turbo = act.turbo ? act.turbo() : false;
  snap.prompt_choice = act.prompt_choice ? act.prompt_choice() : 0;
  snap.prompt_style = act.prompt_style ? act.prompt_style() : 1;
  snap.version_text = act.version_text ? act.version_text() : std::string{};
  return snap;
}

inline std::string BindingLabel(dwg2::input::Binding b, int type, int lang = kLangEnglish) {
  const auto& t = l10n::Get(lang);
  if (!b.kind) return t.bind_unbound;
  if (b.kind == 2) {
    const char* xbox[26] = {
        "A", "B", "X", "Y", "View", "Guide", "Menu", "L3", "R3", "LB", "RB",
        t.bind_dpad_up, t.bind_dpad_down, t.bind_dpad_left, t.bind_dpad_right, "Misc 1",
        "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad",
        "Misc 2", "Misc 3", "Misc 4", "Misc 5", "Misc 6"};
    static constexpr const char* kPs[11] = {
        "Cross", "Circle", "Square", "Triangle", "Create", "PS", "Options",
        "L3", "R3", "L1", "R1"};
    if (b.code < 0 || b.code >= 26) return t.bind_button_unknown;
    return (type == 1 && b.code < 11) ? kPs[b.code] : xbox[b.code];
  }
  if (b.kind == 3) {
    const char* axes[6] = {
        t.bind_ls_x, t.bind_ls_y, t.bind_rs_x, t.bind_rs_y, "LT / L2", "RT / R2"};
    if (b.code < 0 || b.code >= 6) return t.bind_axis_unknown;
    std::string text = axes[b.code];
    if (b.code < 4) text += (b.sign < 0) ? " -" : " +";
    return text;
  }
  std::string text;
  if (b.mods & 1) text += "Shift + ";
  if (b.mods & 2) text += "Ctrl + ";
  if (b.mods & 4) text += "Alt + ";
  if (b.code >= '0' && b.code <= '9') {
    text.push_back(char(b.code));
  } else if (b.code >= 'A' && b.code <= 'Z') {
    text.push_back(char(b.code));
  } else if (b.code >= 112 && b.code <= 123) {
    text += "F" + std::to_string(b.code - 111);
  } else {
    switch (b.code) {
      case 8: text += "Backspace"; break;
      case 9: text += "Tab"; break;
      case 13: text += "Enter"; break;
      case 16: text += "Shift"; break;
      case 17: text += "Ctrl"; break;
      case 18: text += "Alt"; break;
      case 27: text += "Esc"; break;
      case 32: text += t.bind_space; break;
      case 33: text += "Page Up"; break;
      case 34: text += "Page Down"; break;
      case 35: text += "End"; break;
      case 36: text += "Home"; break;
      case 37: text += "Left"; break;
      case 38: text += "Up"; break;
      case 39: text += "Right"; break;
      case 40: text += "Down"; break;
      case 45: text += "Insert"; break;
      case 46: text += "Delete"; break;
      case 186: text += "Semicolon"; break;
      case 187: text += "="; break;
      case 188: text += ","; break;
      case 189: text += "-"; break;
      case 190: text += "."; break;
      case 191: text += "Slash"; break;
      case 192: text += "Grave accent"; break;
      case 219: text += "["; break;
      case 220: text += "Backslash"; break;
      case 221: text += "]"; break;
      case 222: text += "Apostrophe"; break;
      default: text += std::string(t.bind_key_prefix) + std::to_string(b.code); break;
    }
  }
  return text;
}

inline void HandleGamepad(const dwg2::input::Raw& raw, int family, uint64_t now_ms,
                          const Actions& act) {
  if (!IsOpen()) return;
  const int safe_family = (family == 1 || family == 2) ? family : 2;
  const auto profile = dwg2::input::Get(safe_family);
  const bool close_down =
      dwg2::input::Level(profile.bind[dwg2::input::RecompMenu], raw, profile.deadzone) >= 0.65f ||
      dwg2::input::Level(profile.alternate[dwg2::input::RecompMenu], raw, profile.deadzone) >= 0.65f;

  detail::Command cmd;
  {
    auto& s = detail::GetState();
    std::lock_guard lock(s.mutex);
    detail::SyncLanguageLocked(s, act);
    s.last_gp = raw;

    if (s.capturing_action >= 0) {
      if (raw.buttons[4] && !s.blocked_gp.buttons[4]) {
        s.capturing_action = -1;
        capturing.store(false, std::memory_order_release);
        s.prev_gp_close = close_down;
        detail::SetToastLocked(s, l10n::Get(s.active_language).toast_cancelled, now_ms);
        return;
      }
      if (!raw.buttons[4]) s.blocked_gp.buttons[4] = false;

      if (s.wiz_profile == 1 || s.wiz_profile == 2) {
        dwg2::input::Binding bound{};
        for (int b = 0; b < 26; ++b) {
          if (!raw.buttons[b]) s.blocked_gp.buttons[b] = false;
          if (!bound.kind && b != 4 && raw.buttons[b] && !s.blocked_gp.buttons[b]) {
            bound = {2, b, 1, 0};
          }
        }
        for (int a = 0; a < 6; ++a) {
          if (std::abs(raw.axes[a]) < 0.25f) s.blocked_gp.axes[a] = 0.0f;
          if (!bound.kind && std::abs(raw.axes[a]) > 0.65f &&
              std::abs(s.blocked_gp.axes[a]) < 0.25f) {
            bound = {3, a, raw.axes[a] < 0.0f ? -1 : 1, 0};
          }
        }
        if (bound.kind) {
          s.working_profiles[s.wiz_profile].bind[s.capturing_action] = bound;
          s.working_profiles[s.wiz_profile].alternate[s.capturing_action] = {};
          s.capturing_action = -1;
          capturing.store(false, std::memory_order_release);
          s.prev_a = raw.buttons[0];
          s.prev_b = raw.buttons[1];
          s.prev_x = raw.buttons[2];
          s.prev_lb = raw.buttons[9];
          s.prev_rb = raw.buttons[10];
          s.prev_gp_close = close_down;
          detail::SetToastLocked(s, l10n::Get(s.active_language).toast_updated, now_ms);
        }
      }
      return;
    }

    if (s.gp_close_wait_release) {
      if (!close_down) s.gp_close_wait_release = false;
      s.prev_gp_close = close_down;
    } else {
      const bool close_edge = close_down && !s.prev_gp_close;
      s.prev_gp_close = close_down;
      if (close_edge && s.view == View::kMain) {
        detail::SetOpenLocked(s, false, false);
        return;
      }
    }

    const int dpad_v = raw.buttons[12] ? 1 : (raw.buttons[11] ? -1 : 0);
    const int stick_v = (raw.axes[1] > 0.55f) ? 1 : ((raw.axes[1] < -0.55f) ? -1 : 0);
    const int dpad_h = raw.buttons[14] ? 1 : (raw.buttons[13] ? -1 : 0);
    const int stick_h = (raw.axes[0] > 0.55f) ? 1 : ((raw.axes[0] < -0.55f) ? -1 : 0);
    const int v_step = s.gp_v.StepGamepad(dpad_v, stick_v, now_ms);
    const int h_step = s.gp_h.StepGamepad(dpad_h, stick_h, now_ms);

    bool a_edge = raw.buttons[0] && !s.prev_a;
    const bool b_edge = raw.buttons[1] && !s.prev_b;
    const bool x_edge = raw.buttons[2] && !s.prev_x;
    const bool lb_edge = raw.buttons[9] && !s.prev_lb;
    const bool rb_edge = raw.buttons[10] && !s.prev_rb;
    s.prev_a = raw.buttons[0];
    s.prev_b = raw.buttons[1];
    s.prev_x = raw.buttons[2];
    s.prev_lb = raw.buttons[9];
    s.prev_rb = raw.buttons[10];
    if (a_edge) {
      if (s.last_confirm_source == 1 && now_ms >= s.last_confirm_ms &&
          (now_ms - s.last_confirm_ms) < kCrossSourceConfirmDebounceMs) {
        a_edge = false;
      } else {
        s.last_confirm_source = 2;
        s.last_confirm_ms = now_ms;
      }
    }

    const int prof_step = (rb_edge ? 1 : 0) - (lb_edge ? 1 : 0);
    cmd = detail::NavigateLocked(s, v_step, h_step, a_edge, b_edge, x_edge, prof_step, act, now_ms,
                                 safe_family);
  }
  detail::ExecuteCommand(cmd, act, now_ms);
}

inline void HandleKeyboard(const dwg2::input::Raw& raw, uint64_t now_ms, const Actions& act) {
  if (!IsOpen()) return;
  const auto profile = dwg2::input::Get(0);
  const bool close_down =
      dwg2::input::Level(profile.bind[dwg2::input::RecompMenu], raw, 0) >= 0.65f ||
      dwg2::input::Level(profile.alternate[dwg2::input::RecompMenu], raw, 0) >= 0.65f;

  detail::Command cmd;
  {
    auto& s = detail::GetState();
    std::lock_guard lock(s.mutex);
    detail::SyncLanguageLocked(s, act);
    s.last_kb = raw;

    if (s.capturing_action >= 0) {
      if (raw.keys[27] && !s.blocked_keys[27]) {
        s.capturing_action = -1;
        capturing.store(false, std::memory_order_release);
        s.prev_esc = true;
        s.prev_kb_close = close_down;
        detail::SetToastLocked(s, l10n::Get(s.active_language).toast_cancelled, now_ms);
        return;
      }
      if (!raw.keys[27]) s.blocked_keys[27] = false;

      if (s.wiz_profile == 0) {
        dwg2::input::Binding bound{};
        for (int k = 8; k < 255; ++k) {
          if (!raw.keys[k]) s.blocked_keys[k] = false;
          if (!bound.kind && raw.keys[k] && !s.blocked_keys[k] && k != 16 && k != 17 &&
              k != 18 && !(k >= 160 && k <= 165) && k != 27) {
            bound = {1, k, 1, raw.mods & 7};
          }
        }
        if (!bound.kind) {
          for (int k : {16, 17, 18}) {
            const bool down = raw.keys[k];
            if (down && !s.blocked_keys[k]) {
              s.modifier_candidate = k;
              const int bit = (k == 16) ? 1 : ((k == 17) ? 2 : 4);
              s.modifier_bits = (raw.mods & 7) & ~bit;
            } else if (!down && s.modifier_candidate == k) {
              bound = {1, k, 1, s.modifier_bits};
              break;
            }
          }
        }
        if (bound.kind) {
          s.working_profiles[0].bind[s.capturing_action] = bound;
          s.working_profiles[0].alternate[s.capturing_action] = {};
          s.capturing_action = -1;
          capturing.store(false, std::memory_order_release);
          s.prev_enter = raw.keys[13] || raw.keys[32];
          s.prev_esc = raw.keys[27];
          s.prev_back = raw.keys[8];
          s.prev_del = raw.keys[46];
          s.prev_kb_close = close_down;
          detail::SetToastLocked(s, l10n::Get(s.active_language).toast_updated, now_ms);
        }
      }
      return;
    }

    if (s.kb_close_wait_release) {
      if (!close_down) s.kb_close_wait_release = false;
      s.prev_kb_close = close_down;
    } else {
      const bool close_edge = close_down && !s.prev_kb_close;
      s.prev_kb_close = close_down;
      if (close_edge && s.view == View::kMain) {
        detail::SetOpenLocked(s, false, false);
        return;
      }
    }

    const int raw_v = (raw.keys[40] || raw.keys['S'])
                          ? 1
                          : ((raw.keys[38] || raw.keys['W']) ? -1 : 0);
    const int raw_h = (raw.keys[39] || raw.keys['D'])
                          ? 1
                          : ((raw.keys[37] || raw.keys['A']) ? -1 : 0);
    const int v_step = s.kb_v.Step(raw_v, now_ms);
    const int h_step = s.kb_h.Step(raw_h, now_ms);

    const bool enter_down = (raw.keys[13] && !(raw.mods & 4)) || raw.keys[32];
    const bool esc_down = raw.keys[27];
    const bool back_down = raw.keys[8];
    const bool del_down = raw.keys[46];

    bool enter_edge = enter_down && !s.prev_enter;
    const bool esc_edge = esc_down && !s.prev_esc;
    const bool back_edge = back_down && !s.prev_back;
    const bool del_edge = del_down && !s.prev_del;

    s.prev_enter = enter_down;
    s.prev_esc = esc_down;
    s.prev_back = back_down;
    s.prev_del = del_down;
    if (enter_edge) {
      if (s.last_confirm_source == 2 && now_ms >= s.last_confirm_ms &&
          (now_ms - s.last_confirm_ms) < kCrossSourceConfirmDebounceMs) {
        enter_edge = false;
      } else {
        s.last_confirm_source = 1;
        s.last_confirm_ms = now_ms;
      }
    }

    const bool go_back = esc_edge || (s.view != View::kControlsWizard && back_edge);
    const bool clear_wiz = (s.view == View::kControlsWizard) && (del_edge || back_edge);
    cmd = detail::NavigateLocked(s, v_step, h_step, enter_edge, go_back, clear_wiz, 0, act,
                                 now_ms, 0);
  }
  detail::ExecuteCommand(cmd, act, now_ms);
}

inline void UpdateHitRects(const std::vector<HitRect>& rects, View view, int main_scroll,
                           int wiz_scroll) {
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  s.hit_rects = rects;
  s.hit_view = view;
  s.hit_main_scroll = main_scroll;
  s.hit_wiz_scroll = wiz_scroll;
}

inline void InvalidateHitRects() {
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  s.hit_rects.clear();
  s.dragging_hit = kHitNone;
}

inline bool HitLayoutCurrent(const detail::State& s) {
  return s.hit_view == s.view &&
         (s.view != View::kMain || s.hit_main_scroll == s.main_scroll) &&
         (s.view != View::kControlsWizard || s.hit_wiz_scroll == s.wiz_scroll);
}

struct ViewportMetrics {
  float virt_w = 1280.0f;
  float virt_h = 720.0f;
  float scale = 1.0f;
  float ox = 0.0f;
  float oy = 0.0f;
};

inline ViewportMetrics ComputeViewport(float sw, float sh) {
  if (sw <= 1.0f || sh <= 1.0f) return {1280.0f, 720.0f, 1.0f, 0.0f, 0.0f};
  const float virt_h = 720.0f;
  const float virt_w = std::clamp(sw * virt_h / sh, 960.0f, 1600.0f);
  const float scale = std::min(sw / virt_w, sh / virt_h);
  return {virt_w, virt_h, scale, 0.5f * (sw - virt_w * scale), 0.5f * (sh - virt_h * scale)};
}

inline bool ScreenToVirtual(float sx, float sy, float sw, float sh, float& vx, float& vy) {
  if (sw <= 1.0f || sh <= 1.0f) {
    vx = -1.0f;
    vy = -1.0f;
    return false;
  }
  const auto vp = ComputeViewport(sw, sh);
  vx = (sx - vp.ox) / vp.scale;
  vy = (sy - vp.oy) / vp.scale;
  return vx >= 0.0f && vx <= vp.virt_w && vy >= 0.0f && vy <= vp.virt_h;
}

inline float SliderFraction(float x, const HitRect& rect) {
  const float track_start = rect.x0 + kSliderThumbRadius;
  const float span = std::max(1.0f, rect.x1 - kSliderThumbRadius - track_start);
  return std::clamp((x - track_start) / span, 0.0f, 1.0f);
}

inline void HandleMouseMove(float vx, float vy, const Actions& act = {}) {
  if (!IsOpen()) return;
  detail::Command cmd;
  {
    auto& s = detail::GetState();
    std::lock_guard lock(s.mutex);
    detail::SyncLanguageLocked(s, act);
    if (s.capturing_action >= 0 || !HitLayoutCurrent(s)) return;

    if (s.dragging_hit == kHitFovSlider && s.view == View::kMain) {
      const float t = SliderFraction(vx, s.drag_rect);
      const int pct = int(std::lround(t * 20.0f)) * 5;
      cmd = {detail::PendingOp::kSetFov, false, pct};
    } else if (s.dragging_hit == kHitWizDeadzone && s.view == View::kControlsWizard) {
      const float t = SliderFraction(vx, s.drag_rect);
      s.working_profiles[s.wiz_profile].deadzone = int(std::lround(t * 50.0f));
      return;
    } else {
      for (auto it = s.hit_rects.rbegin(); it != s.hit_rects.rend(); ++it) {
        const auto& r = *it;
        if (vx < r.x0 || vx > r.x1 || vy < r.y0 || vy > r.y1) continue;
        if (r.id == kHitMainRow && s.view == View::kMain && r.sub >= 0 &&
            r.sub < kMainItemCount) {
          s.focus = r.sub;
        } else if (r.id == kHitToggleOff || r.id == kHitToggleOn) {
          s.focus = r.sub;
        } else if (r.id == kHitResStop) {
          s.focus = kResolution;
        } else if (r.id == kHitFpsChoice) {
          s.focus = kFps;
        } else if (r.id == kHitPromptChoice) {
          s.focus = kButtonPrompts;
        } else if (r.id == kHitLanguageChoice) {
          s.focus = kLanguage;
        } else if (r.id == kHitQuitYes &&
                   (s.view == View::kConfirmQuit || s.view == View::kConfirmRestart ||
                    s.view == View::kConfirmLanguageRestart)) {
          s.confirm_quit_yes = true;
        } else if (r.id == kHitQuitNo &&
                   (s.view == View::kConfirmQuit || s.view == View::kConfirmRestart ||
                    s.view == View::kConfirmLanguageRestart)) {
          s.confirm_quit_yes = false;
        } else if (r.id == kHitWizRow && s.view == View::kControlsWizard && r.sub >= 0 &&
                   r.sub < dwg2::input::Count) {
          s.wiz_cursor = r.sub;
        }
        break;
      }
    }
  }
  detail::ExecuteCommand(cmd, act, 0);
}

inline bool HandleMouseDown(float vx, float vy, const Actions& act, uint64_t now_ms = 0) {
  if (!IsOpen()) return false;
  detail::Command cmd;
  bool handled = false;
  {
    auto& s = detail::GetState();
    std::lock_guard lock(s.mutex);
    detail::SyncLanguageLocked(s, act);
    if (s.capturing_action >= 0 || !HitLayoutCurrent(s)) return false;

    for (auto it = s.hit_rects.rbegin(); it != s.hit_rects.rend(); ++it) {
      const auto& r = *it;
      if (vx < r.x0 || vx > r.x1 || vy < r.y0 || vy > r.y1) continue;
      handled = true;
      switch (r.id) {
        case kHitClose:
          if (s.view == View::kMain) detail::SetOpenLocked(s, false, false);
          break;
        case kHitMainRow:
          if (s.view == View::kMain && r.sub >= 0 && r.sub < kMainItemCount) {
            s.focus = r.sub;
            cmd = detail::BuildMainActivateLocked(s, r.sub, act, 0);
          }
          break;
        case kHitMainScrollUp:
          if (s.view == View::kMain) {
            s.main_scroll = std::clamp(s.main_scroll - 1, 0, MaxMainScroll());
            detail::ClampMainFocusToViewLocked(s);
          }
          break;
        case kHitMainScrollDown:
          if (s.view == View::kMain) {
            s.main_scroll = std::clamp(s.main_scroll + 1, 0, MaxMainScroll());
            detail::ClampMainFocusToViewLocked(s);
          }
          break;
        case kHitResStop:
          if (s.view == View::kMain && r.sub >= 1 && r.sub <= 3) {
            s.focus = kResolution;
            cmd = {detail::PendingOp::kSetResolution, false, r.sub};
          }
          break;
        case kHitFovSlider:
          if (s.view == View::kMain) {
            s.focus = kFov;
            s.dragging_hit = kHitFovSlider;
            s.drag_rect = r;
            const float t = SliderFraction(vx, r);
            cmd = {detail::PendingOp::kSetFov, false, int(std::lround(t * 20.0f)) * 5};
          }
          break;
        case kHitFpsChoice:
          if (s.view == View::kMain && dwg2::video::ValidFps(r.sub)) {
            s.focus = kFps;
            cmd = {detail::PendingOp::kSetFps, false, r.sub};
          }
          break;
        case kHitPromptChoice:
          if (s.view == View::kMain) {
            if (r.sub == -1 || r.sub == -2) {
              s.focus = kButtonPrompts;
              const int cur = act.prompt_choice ? act.prompt_choice() : 0;
              const int dir = (r.sub == -1) ? -1 : 1;
              cmd = {detail::PendingOp::kSetPrompts, false,
                     detail::StepChoice(dwg2::prompts::kMenuOrder, 9, cur, dir, true)};
            } else if (dwg2::prompts::ValidChoice(r.sub)) {
              s.focus = kButtonPrompts;
              cmd = {detail::PendingOp::kSetPrompts, false, r.sub};
            }
          }
          break;
        case kHitLanguageChoice:
          if (s.view == View::kMain) {
            s.focus = kLanguage;
            if (r.sub == -1 || r.sub == -2) {
              const int cur = s.pending_language_dirty ? s.pending_language : s.active_language;
              const int dir = (r.sub == -1) ? -1 : 1;
              s.pending_language = detail::StepChoice(kLanguageOrder, 5, cur, dir, true);
              s.pending_language_dirty = true;
            } else if (r.sub == 0) {
              cmd = detail::BuildMainActivateLocked(s, kLanguage, act, 0);
            }
          }
          break;
        case kHitToggleOff:
        case kHitToggleOn:
          if (s.view == View::kMain) {
            s.focus = r.sub;
            cmd = detail::BuildMainHorizLocked(s, r.sub, r.id == kHitToggleOn ? 1 : -1, act);
          }
          break;
        case kHitQuitYes:
        case kHitQuitNo:
          if (s.view == View::kConfirmQuit || s.view == View::kConfirmRestart ||
              s.view == View::kConfirmLanguageRestart) {
            cmd = detail::NavigateLocked(s, 0, r.id == kHitQuitYes ? -1 : 1, true,
                                         false, false, 0, act, now_ms, 0);
          }
          break;
        case kHitWizPrevProfile:
          if (s.view == View::kControlsWizard) s.wiz_profile = (s.wiz_profile + 2) % 3;
          break;
        case kHitWizNextProfile:
          if (s.view == View::kControlsWizard) s.wiz_profile = (s.wiz_profile + 1) % 3;
          break;
        case kHitWizProfileTab:
          if (s.view == View::kControlsWizard && r.sub >= 0 && r.sub <= 2) {
            s.wiz_profile = r.sub;
          }
          break;
        case kHitWizRow:
          if (s.view == View::kControlsWizard && r.sub >= 0 && r.sub < dwg2::input::Count) {
            s.wiz_cursor = r.sub;
            detail::EnsureWizScrollLocked(s);
            detail::BeginCaptureLocked(s, r.sub, now_ms);
          }
          break;
        case kHitWizDeadzone:
          if (s.view == View::kControlsWizard) {
            s.wiz_cursor = kWizDeadzone;
            s.dragging_hit = kHitWizDeadzone;
            s.drag_rect = r;
            const float t = SliderFraction(vx, r);
            s.working_profiles[s.wiz_profile].deadzone = int(std::lround(t * 50.0f));
          }
          break;
        case kHitWizDefaults:
          if (s.view == View::kControlsWizard) {
            s.wiz_cursor = kWizDefaults;
            s.working_profiles[s.wiz_profile] = dwg2::input::Defaults(s.wiz_profile);
            detail::SetToastLocked(s, l10n::Get(s.active_language).toast_defaults, now_ms);
          }
          break;
        case kHitWizSave:
          if (s.view == View::kControlsWizard) {
            s.wiz_cursor = kWizSave;
            cmd = {detail::PendingOp::kSaveWizard};
          }
          break;
        case kHitWizBack:
          if (s.view == View::kControlsWizard) {
            s.view = View::kMain;
            s.focus = kConfigureControls;
            detail::EnsureMainScrollLocked(s);
          }
          break;
        default:
          handled = false;
          break;
      }
      if (handled) break;
    }
  }
  detail::ExecuteCommand(cmd, act, now_ms);
  return handled;
}

inline void HandleMouseUp() {
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  s.dragging_hit = kHitNone;
}

inline void HandleMouseWheel(float delta) {
  if (!IsOpen() || delta == 0.0f) return;
  auto& s = detail::GetState();
  std::lock_guard lock(s.mutex);
  if (s.capturing_action >= 0) return;
  const int step = delta > 0.0f ? -1 : 1;
  if (s.view == View::kMain) {
    s.main_scroll = std::clamp(s.main_scroll + step, 0, MaxMainScroll());
    detail::ClampMainFocusToViewLocked(s);
  } else if (s.view == View::kControlsWizard) {
    const int max_scroll = std::max(0, dwg2::input::Count - kWizVisibleRows);
    s.wiz_scroll = std::clamp(s.wiz_scroll + step, 0, max_scroll);
  }
}

}  // namespace dwg2::recomp_menu
