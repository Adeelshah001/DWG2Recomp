#include "hooks.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <windows.h>
#include <mmsystem.h>

#include <rex/chrono/clock.h>
#include "fixed60.h"
#include "video_control.h"

extern "C" __declspec(dllimport) uint64_t Dwg2PauseActiveTicks();

REX_EXTERN(__imp__sub_82119380);
REX_EXTERN(__imp__sub_82545F60);
REX_EXTERN(__imp__sub_82225298);
REX_EXTERN(__imp__sub_82386348);
REX_EXTERN(__imp__sub_82374848);
REX_EXTERN(__imp__sub_822F9680);
REX_EXTERN(__imp__sub_82302148);
REX_EXTERN(__imp__sub_822F9750);
REX_EXTERN(__imp__sub_82302998);
REX_EXTERN(__imp__sub_821A0578);
REX_EXTERN(__imp__sub_823FAC30);
REX_EXTERN(__imp__sub_82152670);
REX_EXTERN(__imp__sub_821524C0);

namespace {

using BattleClock = std::chrono::steady_clock;

thread_local bool fixed_battle_step = false;
thread_local bool measure_battle_swap = false;
thread_local bool outer_battle_swap = false;
std::atomic<bool> battle_swap_unlock{false};
std::atomic<bool> particle_worker_empty_parked{false};
std::atomic<int> last_queued_interval{-1}, last_callback_interval{-1};
std::atomic<unsigned> battle_ticks{0}, battle_draws{0};
thread_local BattleClock::duration outer_swap_gpu_wait{};
thread_local unsigned outer_swap_poll_count = 0;
thread_local dwg2::timing::Fixed60 battle_clock;
thread_local uint64_t battle_sample_ticks{};

std::chrono::nanoseconds BattleElapsed(uint64_t active_ticks) {
  static const double tick_frequency = double(rex::chrono::Clock::QueryHostTickFrequency());
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(double(active_ticks - battle_sample_ticks) / tick_frequency));
}

struct BattleViewSample {
  uint32_t view{};
  uint32_t view_camera{};
  std::array<float, 16> matrix{};
  std::array<float, 16> inv_pose{};
  dwg2::timing::CameraMatrix pose{}, projection{};
  bool pose_product_matches{};
  bool has_reflection{};
  std::array<float, 16> reflection_matrix{};
  dwg2::timing::CameraMatrix reflection_pose{}, reflection_projection{};
  bool reflection_product_matches{};
};

thread_local BattleViewSample previous_battle_view{}, current_battle_view{};
thread_local dwg2::timing::CameraMatrix tick_pose{}, tick_projection{};
thread_local uint32_t tick_projection_view = 0;
thread_local bool view_camera_overridden = false;
thread_local uint32_t last_written_view_camera = 0;
thread_local std::array<float, 16> last_written_matrix{};

struct ScopedTimerPeriod {
  bool active = false;
  void Set(bool enable) {
    if (enable == active) return;
    if (enable) {
      active = (timeBeginPeriod(1) == TIMERR_NOERROR);
    } else {
      timeEndPeriod(1);
      active = false;
    }
  }
  ~ScopedTimerPeriod() { Set(false); }
};

void WaitUntilPaced(BattleClock::time_point target) {
  const auto now = BattleClock::now();
  if (target <= now) return;
  constexpr auto kSpinWindow = std::chrono::milliseconds(2);
  if (target - now > kSpinWindow) {
    std::this_thread::sleep_until(target - kSpinWindow);
  }
  while (BattleClock::now() < target) {
    YieldProcessor();
  }
}

int Dwg2EnvOverride(const wchar_t* name) {
  wchar_t value[4]{};
  const auto len = GetEnvironmentVariableW(name, value, 4);
  if (len == 1 && value[0] == L'0') return 0;
  if (len == 1 && value[0] == L'1') return 1;
  return -1;
}

bool Dwg2EnvEnabled(const wchar_t* name, bool default_value) {
  const int override_val = Dwg2EnvOverride(name);
  return override_val < 0 ? default_value : override_val != 0;
}

int Dwg2CurrentFpsSetting() {
  using QueryFn = int (*)(int);
  static const QueryFn fn = []() -> QueryFn {
    if (const auto mod = GetModuleHandleW(L"rexruntime.dll"))
      return reinterpret_cast<QueryFn>(GetProcAddress(mod, "Dwg2GraphicsValue"));
    return nullptr;
  }();
  return fn ? dwg2::video::NormalizeFps(fn(5)) : dwg2::video::kDefaultFps;
}

bool Dwg2ImmediateBattleSwapActive() {
  static const int env_override = Dwg2EnvOverride(L"DWG2_EXPERIMENTAL_IMMEDIATE_SWAP");
  if (env_override >= 0) return env_override != 0;
  return dwg2::timing::ShouldUnlockBattleSwap(Dwg2CurrentFpsSetting());
}

void SyncBattlePacing(bool battle_active) {
  using SetVsyncFn = int (*)(int, int);
  static const SetVsyncFn fn = []() -> SetVsyncFn {
    if (const auto mod = GetModuleHandleW(L"rexruntime.dll"))
      return reinterpret_cast<SetVsyncFn>(GetProcAddress(mod, "Dwg2SetBattlePacing"));
    return nullptr;
  }();
  const int desired = (battle_active && Dwg2ImmediateBattleSwapActive()) ? 0 : 1;
  const int state = (int(battle_active) << 1) | desired;
  static std::atomic<int> current_state{-1};
  if (current_state.load(std::memory_order_relaxed) == state) return;
  if (!fn) {
    current_state.store(state, std::memory_order_relaxed);
    REXLOG_WARN("DWG2 battle pacing: Dwg2SetBattlePacing export unavailable");
    return;
  }
  const int fps = Dwg2CurrentFpsSetting();
  const int applied = fn(battle_active, desired);
  if (applied) current_state.store(state, std::memory_order_relaxed);
  REXLOG_INFO("DWG2 GPU vsync: battle_active={} fps_cap={} vsync={} applied={}", battle_active, fps,
              desired != 0, applied != 0);
}

// Wait until the particle worker finishes consuming queued batches before freeing a trail
// or running consecutive fixed catch-up ticks. Bounded at 50 ms to never deadlock shutdown.
void WaitForParticleWorkerIdle(uint8_t* base, bool allow_open_batch_park) {
  const uint32_t qm = REX_LOAD_U32(0x826F77A0);
  if (!qm || REX_LOAD_U32(qm + 28) != 0) return;
  auto is_safe = [&](uint32_t& pending, uint32_t& ring_count, bool& parked) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    pending = REX_LOAD_U32(qm + 32);
    parked = particle_worker_empty_parked.load(std::memory_order_acquire);
    const uint32_t ring = REX_LOAD_U32(qm + 20);
    ring_count = ring ? REX_LOAD_U32(ring + 36) : 1u;
    return dwg2::timing::ParticleWorkerSafeForTrailFree(pending, allow_open_batch_park, parked,
                                                        ring_count);
  };
  uint32_t pending = 0, ring_count = 0;
  bool parked = false;
  if (is_safe(pending, ring_count, parked)) return;
  const auto deadline = BattleClock::now() + std::chrono::milliseconds(50);
  unsigned spins = 0;
  while (!is_safe(pending, ring_count, parked)) {
    if (REX_LOAD_U32(qm + 28) != 0) return;
    if (BattleClock::now() >= deadline) {
      static std::atomic<ULONGLONG> last_warn_ms{0};
      static std::atomic<uint64_t> suppressed{0};
      const ULONGLONG now_ms = GetTickCount64();
      ULONGLONG prev_ms = last_warn_ms.load(std::memory_order_relaxed);
      if ((prev_ms == 0 || now_ms - prev_ms >= 5000) &&
          last_warn_ms.compare_exchange_strong(prev_ms, now_ms, std::memory_order_relaxed)) {
        const uint64_t dropped = suppressed.exchange(0, std::memory_order_relaxed);
        REXLOG_WARN(
            "DWG2 particle queue wait timed out (pending={} ring_count={} parked={} "
            "allow_open_batch_park={} suppressed={})",
            pending, ring_count, parked, allow_open_batch_park, dropped);
        return;
      }
      suppressed.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (spins++ < 64) {
      YieldProcessor();
    } else {
      SwitchToThread();
    }
  }
}

void CaptureBattleView(uint8_t* base) {
  const auto root = (REX_LOAD_U32(0x826CB400) & 1) != 0 ? 0x826C4F00u : 0u;
  const auto owner = root ? REX_LOAD_U32(root + 23760) : 0u;
  const auto view = owner ? REX_LOAD_U32(owner + 68) : 0u;
  const auto view_camera = view ? REX_LOAD_U32(view + 40) : 0u;
  BattleViewSample sample{};
  if (view && view_camera) {
    sample.view = view;
    sample.view_camera = view_camera;
    constexpr uint32_t camera_data = 0x826C52F0u;
    for (unsigned i = 0; i < sample.projection.size(); ++i) {
      sample.pose[i] = std::bit_cast<float>(REX_LOAD_U32(camera_data + 112 + 4 * i));
      sample.projection[i] = std::bit_cast<float>(REX_LOAD_U32(camera_data + 176 + 4 * i));
      sample.matrix[i] = std::bit_cast<float>(REX_LOAD_U32(view_camera + 16 + 4 * i));
      sample.inv_pose[i] = std::bit_cast<float>(REX_LOAD_U32(view_camera + 80 + 4 * i));
      if (!std::isfinite(sample.pose[i]) || !std::isfinite(sample.projection[i]) ||
          !std::isfinite(sample.matrix[i]) || !std::isfinite(sample.inv_pose[i])) {
        sample.view = 0;
        break;
      }
    }
    if (sample.view) {
      sample.pose_product_matches = dwg2::timing::MatrixMatches(
          dwg2::timing::ViewProjection(sample.pose, sample.projection), sample.matrix);
      if (tick_projection_view == view) {
        sample.has_reflection = true;
        sample.reflection_projection = tick_projection;
        sample.reflection_pose = tick_pose;
        for (unsigned i = 0; i < sample.reflection_matrix.size(); ++i) {
          sample.reflection_matrix[i] = std::bit_cast<float>(REX_LOAD_U32(view + 64 + 4 * i));
          if (!std::isfinite(sample.reflection_matrix[i])) {
            sample.has_reflection = false;
            break;
          }
        }
        if (sample.has_reflection) {
          sample.reflection_product_matches = dwg2::timing::MatrixMatches(
              dwg2::timing::ViewProjection(sample.reflection_pose, sample.reflection_projection),
              sample.reflection_matrix);
        }
      }
    }
  }
  previous_battle_view = current_battle_view;
  current_battle_view = sample;
  view_camera_overridden = false;
}

constexpr uint32_t battle_manager = 0x826C3998;
bool BattleCanStep(uint8_t* base) {
  if (REX_LOAD_U32(battle_manager + 8) != 14 || REX_LOAD_U32(battle_manager + 12) != 52)
    return false;
  const auto scene = REX_LOAD_U32(battle_manager + 20);
  return scene && REX_LOAD_U32(scene) == 0x82036194;
}

}  // namespace

REX_HOOK_RAW(sub_823FAC30) {
  std::atomic_thread_fence(std::memory_order_acquire);
  const uint32_t qm = REX_LOAD_U32(0x826F77A0);
  const uint32_t ring = ctx.r3.u32;
  if (qm != 0 && ring != 0 && ring == REX_LOAD_U32(qm + 20) && REX_LOAD_U32(qm + 32) != 0 &&
      REX_LOAD_U32(qm + 28) == 0 && REX_LOAD_U32(ring + 36) == 0) {
    particle_worker_empty_parked.store(true, std::memory_order_seq_cst);
    unsigned spins = 0;
    for (;;) {
      std::atomic_thread_fence(std::memory_order_acquire);
      if (REX_LOAD_U32(qm + 28) != 0 || REX_LOAD_U32(qm + 32) == 0 ||
          REX_LOAD_U32(ring + 36) != 0) {
        break;
      }
      if (spins++ < 64) {
        YieldProcessor();
      } else {
        SwitchToThread();
      }
    }
    particle_worker_empty_parked.store(false, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }
  __imp__sub_823FAC30(ctx, base);
}

REX_HOOK_RAW(sub_821A0578) {
  WaitForParticleWorkerIdle(base, true);
  __imp__sub_821A0578(ctx, base);
}

REX_HOOK_RAW(sub_82152670) {
  battle_ticks.fetch_add(1, std::memory_order_relaxed);
  __imp__sub_82152670(ctx, base);
}

REX_HOOK_RAW(sub_821524C0) {
  __imp__sub_821524C0(ctx, base);
  const auto now = BattleClock::now();
  static thread_local auto start = now;
  battle_draws.fetch_add(1, std::memory_order_relaxed);
  const double seconds = std::chrono::duration<double>(now - start).count();
  if (seconds >= 5.0) {
    REXLOG_INFO("DWG2 battle timing: scene updates {:.2f}/s, scene draw calls {:.2f}/s",
                battle_ticks.exchange(0, std::memory_order_relaxed) / seconds,
                battle_draws.exchange(0, std::memory_order_relaxed) / seconds);
    start = now;
  }
}

REX_HOOK_RAW(sub_82545F60) {
  const auto return_address = ctx.lr;
  __imp__sub_82545F60(ctx, base);
  // This pair feeds both accumulated scene time and the virtual tick's delta.
  if (fixed_battle_step && return_address == 0x82128B00) ctx.f1.f64 = 1.0;
  if (fixed_battle_step && return_address == 0x82128B14) ctx.f1.f64 = 60.0;
}

REX_HOOK_RAW(sub_82119380) {
  static const bool enabled = Dwg2EnvEnabled(L"DWG2_EXPERIMENTAL_FIXED_BATTLE", true);
  if (!enabled || ctx.lr != 0x821107A8 || ctx.r3.u32 != battle_manager || !BattleCanStep(base)) {
    measure_battle_swap = false;
    battle_swap_unlock.store(false, std::memory_order_relaxed);
    SyncBattlePacing(false);
    previous_battle_view = current_battle_view = {};
    view_camera_overridden = false;
    battle_clock.reset();
    battle_sample_ticks = 0;
    __imp__sub_82119380(ctx, base);
    return;
  }
  measure_battle_swap = true;
  battle_swap_unlock.store(true, std::memory_order_relaxed);
  SyncBattlePacing(true);
  const uint64_t active_ticks = Dwg2PauseActiveTicks();
  const auto elapsed = battle_sample_ticks ? BattleElapsed(active_ticks)
                                           : std::chrono::nanoseconds(16'666'667);
  battle_sample_ticks = active_ticks;
  const auto plan = battle_clock.advance(elapsed);
  if (plan.discarded_ticks) {
    REXLOG_WARN("DWG2 fixed battle: dropped {} catch-up ticks", plan.discarded_ticks);
  }
  if (!plan.ticks) {
    // The outer loop only consumes the continue/exit result of this call.
    ctx.r3.u64 = 1;
    return;
  }
  const auto input = ctx;
  for (unsigned i = 0; i < plan.ticks; ++i) {
    if (i) {
      WaitForParticleWorkerIdle(base, false);
      ctx = input;
    }
    tick_projection_view = 0;
    fixed_battle_step = true;
    __imp__sub_82119380(ctx, base);
    fixed_battle_step = false;
    if (!ctx.r3.u32 || !BattleCanStep(base)) {
      measure_battle_swap = false;
      battle_swap_unlock.store(false, std::memory_order_relaxed);
      SyncBattlePacing(false);
      previous_battle_view = current_battle_view = {};
      view_camera_overridden = false;
      battle_clock.reset();
      battle_sample_ticks = 0;
      break;
    }
    CaptureBattleView(base);
  }
}

REX_HOOK_RAW(sub_82225298) {
  const auto view = ctx.r3.u32;
  const bool sample = (ctx.lr == 0x82152090 || ctx.lr == 0x821521E0) && view &&
                      previous_battle_view.view == view && current_battle_view.view == view &&
                      current_battle_view.view_camera != 0;
  __imp__sub_82225298(ctx, base);
  if (!sample) return;

  const auto view_camera = current_battle_view.view_camera;
  const auto& expected_upload =
      (view_camera_overridden && last_written_view_camera == view_camera)
          ? last_written_matrix
          : current_battle_view.matrix;
  bool matches = true;
  for (unsigned i = 0; i < expected_upload.size(); ++i) {
    const float uploaded = std::bit_cast<float>(REX_LOAD_U32(view_camera + 16 + 4 * i));
    matches &= std::bit_cast<uint32_t>(expected_upload[i]) == std::bit_cast<uint32_t>(uploaded);
  }

  static const bool interpolate = Dwg2EnvEnabled(L"DWG2_EXPERIMENTAL_CAMERA_INTERPOLATION", true);
  const auto battle_render_alpha = battle_clock.render_alpha(BattleElapsed(Dwg2PauseActiveTicks()));
  const bool same_projection = previous_battle_view.projection == current_battle_view.projection;
  const bool product_matches = current_battle_view.pose_product_matches;
  auto pose = current_battle_view.pose;

  bool applied = false;
  bool has_inv_pose = false;
  std::array<float, 16> interpolated = current_battle_view.matrix;
  std::array<float, 16> interpolated_inv_pose = current_battle_view.inv_pose;
  if (interpolate && matches && same_projection && product_matches &&
      dwg2::timing::InterpolateCameraPose(previous_battle_view.pose, pose,
                                          static_cast<float>(battle_render_alpha))) {
    interpolated = dwg2::timing::ViewProjection(pose, current_battle_view.projection);
    interpolated_inv_pose = dwg2::timing::InvertCameraPose(pose);
    applied = true;
    has_inv_pose = true;
  } else if (interpolate && matches &&
             dwg2::timing::InterpolateViewTranslation(previous_battle_view.matrix, interpolated,
                                                      static_cast<float>(battle_render_alpha))) {
    applied = true;
  }

  bool uploaded_pose = false;
  const auto all_finite = [](const std::array<float, 16>& m) {
    return std::all_of(m.begin(), m.end(), [](float v) { return std::isfinite(v); });
  };
  if (applied && battle_render_alpha < 1.0 && all_finite(interpolated) &&
      (!has_inv_pose || all_finite(interpolated_inv_pose))) {
    uploaded_pose = true;
    for (unsigned i = 0; i < interpolated.size(); ++i) {
      REX_STORE_U32(view_camera + 16 + 4 * i, std::bit_cast<uint32_t>(interpolated[i]));
      if (has_inv_pose)
        REX_STORE_U32(view_camera + 80 + 4 * i, std::bit_cast<uint32_t>(interpolated_inv_pose[i]));
    }
    view_camera_overridden = true;
    last_written_view_camera = view_camera;
    last_written_matrix = interpolated;

    if (previous_battle_view.has_reflection && current_battle_view.has_reflection) {
      auto refl_pose = current_battle_view.reflection_pose;
      auto refl_mat = current_battle_view.reflection_matrix;
      bool refl_ok = false;
      if (previous_battle_view.reflection_projection ==
              current_battle_view.reflection_projection &&
          current_battle_view.reflection_product_matches &&
          dwg2::timing::InterpolateCameraPose(previous_battle_view.reflection_pose, refl_pose,
                                              static_cast<float>(battle_render_alpha))) {
        refl_mat =
            dwg2::timing::ViewProjection(refl_pose, current_battle_view.reflection_projection);
        refl_ok = true;
      } else if (dwg2::timing::InterpolateViewTranslation(
                     previous_battle_view.reflection_matrix, refl_mat,
                     static_cast<float>(battle_render_alpha))) {
        refl_ok = true;
      }
      if (refl_ok && all_finite(refl_mat)) {
        const auto graphics = REX_LOAD_U32(view + 4);
        if (graphics) {
          for (unsigned i = 0; i < refl_mat.size(); ++i)
            REX_STORE_U32(graphics + 4 * i, std::bit_cast<uint32_t>(refl_mat[i]));
        }
      }
    }
  } else if (view_camera_overridden && last_written_view_camera == view_camera) {
    for (unsigned i = 0; i < current_battle_view.matrix.size(); ++i) {
      REX_STORE_U32(view_camera + 16 + 4 * i,
                    std::bit_cast<uint32_t>(current_battle_view.matrix[i]));
      REX_STORE_U32(view_camera + 80 + 4 * i,
                    std::bit_cast<uint32_t>(current_battle_view.inv_pose[i]));
    }
    view_camera_overridden = false;
  }

  if (REXCVAR_GET(dwg2_diag_trace)) {
    static thread_local ULONGLONG last_log{};
    static thread_local unsigned window_samples{}, window_uploaded{};
    ++window_samples;
    if (uploaded_pose) ++window_uploaded;
    const auto now = GetTickCount64();
    if (now - last_log >= 1000) {
      last_log = now;
      REXLOG_INFO(
          "DWG2 battle view: snapshot_matches_upload={} alpha={} same_projection={} "
          "product_matches={} pose_eligible={} pose_uploaded={} interpolated={}/{}",
          matches, battle_render_alpha, same_projection, product_matches, applied, uploaded_pose,
          window_uploaded, window_samples);
      window_samples = 0;
      window_uploaded = 0;
    }
  }
}

REX_HOOK_RAW(sub_822F9680) {
  if (!outer_battle_swap) {
    __imp__sub_822F9680(ctx, base);
    return;
  }
  outer_swap_poll_count = 0;
  const auto start = BattleClock::now();
  __imp__sub_822F9680(ctx, base);
  outer_swap_gpu_wait += BattleClock::now() - start;
}

REX_HOOK_RAW(sub_82302148) {
  // The Xbox delay instructions in this poll compile away on the host.
  if (outer_battle_swap && ctx.lr == 0x822F9718) {
    if (++outer_swap_poll_count <= 64) {
      YieldProcessor();
    } else {
      SwitchToThread();
    }
  }
  __imp__sub_82302148(ctx, base);
}

REX_HOOK_RAW(sub_82374848) {
  static thread_local ScopedTimerPeriod timer_res;
  static thread_local BattleClock::time_point window{}, next_guest_swap{};
  static thread_local BattleClock::duration in_swap{}, gpu_wait{};
  static thread_local unsigned swaps = 0;
  static thread_local int prior_cap = -1;
  const bool measure = measure_battle_swap && ctx.lr == 0x82110798;
  if (!measure) {
    if (ctx.lr == 0x82110798) {
      swaps = 0;
      in_swap = {};
      gpu_wait = {};
      next_guest_swap = {};
      prior_cap = -1;
      timer_res.Set(false);
    }
    if (!battle_swap_unlock.load(std::memory_order_relaxed)) SyncBattlePacing(false);
    __imp__sub_82374848(ctx, base);
    return;
  }
  SyncBattlePacing(battle_swap_unlock.load(std::memory_order_relaxed));
  const auto start = BattleClock::now();
  if (!swaps) window = start;
  outer_swap_gpu_wait = {};
  outer_swap_poll_count = 0;
  outer_battle_swap = true;
  __imp__sub_82374848(ctx, base);
  outer_battle_swap = false;
  const auto swap_done = BattleClock::now();
  in_swap += swap_done - start;
  gpu_wait += outer_swap_gpu_wait;

  const int fps_cap = Dwg2CurrentFpsSetting();
  timer_res.Set(fps_cap > 0);
  if (fps_cap > 0) {
    const auto interval = std::chrono::duration_cast<BattleClock::duration>(
        std::chrono::duration<double>(1.0 / fps_cap));
    if (fps_cap != prior_cap || swap_done > next_guest_swap + interval) {
      next_guest_swap = swap_done;
    }
    if (next_guest_swap > swap_done) {
      WaitUntilPaced(next_guest_swap);
    }
    next_guest_swap += interval;
  } else {
    next_guest_swap = swap_done;
  }
  prior_cap = fps_cap;

  const auto end = BattleClock::now();
  if (++swaps == 120) {
    const double wall_ms = std::chrono::duration<double, std::milli>(end - window).count();
    const double swap_ms = std::chrono::duration<double, std::milli>(in_swap).count();
    const double wait_ms = std::chrono::duration<double, std::milli>(gpu_wait).count();
    const double swap_hz = wall_ms > 0.0 ? (swaps * 1000.0) / wall_ms : 0.0;
    REXLOG_INFO(
        "DWG2 guest swap: count={} wall_ms={:.2f} in_swap_ms={:.2f} gpu_wait_ms={:.2f} "
        "swaps_per_s={:.2f} unlock={} queued_interval={} callback_interval={}",
        swaps, wall_ms, swap_ms, wait_ms, swap_hz,
        battle_swap_unlock.load(std::memory_order_relaxed) && Dwg2ImmediateBattleSwapActive(),
        last_queued_interval.load(std::memory_order_relaxed),
        last_callback_interval.load(std::memory_order_relaxed));
    swaps = 0;
    in_swap = {};
    gpu_wait = {};
  }
}

REX_HOOK_RAW(sub_822F9750) {
  if (ctx.lr == 0x82302C94 || ctx.lr == 0x8230C9C8) {
    const auto interval = (ctx.r7.u32 >> 8) & 0xf;
    const bool battle_active = outer_battle_swap || battle_swap_unlock.load(std::memory_order_relaxed);
    const bool unlock = battle_active && Dwg2ImmediateBattleSwapActive();
    if (unlock) ctx.r7.u32 = dwg2::timing::ImmediateBattleSwap(ctx.r7.u32);
    const auto effective = (ctx.r7.u32 >> 8) & 0xf;
    last_queued_interval.store(static_cast<int>(effective), std::memory_order_relaxed);
    const int state = (int(unlock) << 8) | (int(interval) << 4) | int(effective);
    static thread_local int last_state = -1;
    if (state != last_state && (unlock || effective != interval || last_state != -1)) {
      last_state = state;
      REXLOG_INFO(
          "DWG2 guest swap queue: unlock={} original_interval={} effective_interval={} "
          "return=0x{:08X}",
          unlock, interval, effective, static_cast<uint32_t>(ctx.lr));
    }
  }
  __imp__sub_822F9750(ctx, base);
}

REX_HOOK_RAW(sub_82302998) {
  const auto original_interval = (ctx.r3.u32 >> 8) & 0xf;
  const bool battle_active = battle_swap_unlock.load(std::memory_order_relaxed);
  const bool unlock = battle_active && Dwg2ImmediateBattleSwapActive();
  if (unlock) ctx.r3.u32 = dwg2::timing::ImmediateBattleSwap(ctx.r3.u32);
  const auto effective = (ctx.r3.u32 >> 8) & 0xf;
  last_callback_interval.store(static_cast<int>(effective), std::memory_order_relaxed);
  const int state = (int(unlock) << 8) | (int(original_interval) << 4) | int(effective);
  static thread_local int last_state = -1;
  if (state != last_state && (BattleCanStep(base) || last_state != -1)) {
    last_state = state;
    REXLOG_INFO(
        "DWG2 guest swap callback: unlock={} original_interval={} effective_interval={} "
        "return=0x{:08X}",
        unlock, original_interval, effective, static_cast<uint32_t>(ctx.lr));
  }
  __imp__sub_82302998(ctx, base);
}

REX_HOOK_RAW(sub_82386348) {
  const auto camera = ctx.r3.u32;
  const auto view = ctx.r29.u32;
  const bool capture = fixed_battle_step && ctx.lr == 0x82225844 && ctx.r28.u32 == 0 && view;
  __imp__sub_82386348(ctx, base);
  if (!capture) return;

  for (unsigned i = 0; i < tick_projection.size(); ++i) {
    tick_projection[i] = std::bit_cast<float>(REX_LOAD_U32(camera + 192 + 4 * i));
    tick_pose[i] = std::bit_cast<float>(REX_LOAD_U32(camera + 128 + 4 * i));
    if (!std::isfinite(tick_projection[i]) || !std::isfinite(tick_pose[i])) return;
  }
  tick_projection_view = view;
}

namespace dwg2::hooks {

void RegisterTimingFpsHooks(rex::runtime::FunctionDispatcher* dispatcher) {
  if (dispatcher->GetFunction(0x82119380) != &sub_82119380 ||
      dispatcher->GetFunction(0x82545F60) != &sub_82545F60 ||
      dispatcher->GetFunction(0x82225298) != &sub_82225298 ||
      dispatcher->GetFunction(0x82386348) != &sub_82386348 ||
      dispatcher->GetFunction(0x82374848) != &sub_82374848 ||
      dispatcher->GetFunction(0x822F9680) != &sub_822F9680 ||
      dispatcher->GetFunction(0x82302148) != &sub_82302148 ||
      dispatcher->GetFunction(0x822F9750) != &sub_822F9750 ||
      dispatcher->GetFunction(0x82302998) != &sub_82302998 ||
      dispatcher->GetFunction(0x821A0578) != &sub_821A0578 ||
      dispatcher->GetFunction(0x823FAC30) != &sub_823FAC30 ||
      dispatcher->GetFunction(0x82152670) != &sub_82152670 ||
      dispatcher->GetFunction(0x821524C0) != &sub_821524C0) {
    throw std::runtime_error("DWG2 timing/FPS hooks do not match this game build");
  }
}

}  // namespace dwg2::hooks
