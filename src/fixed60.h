#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace dwg2::timing {
constexpr std::uint32_t ImmediateBattleSwap(std::uint32_t flags) {
  const auto interval = (flags >> 8) & 0xf;
  return interval >= 1 && interval <= 3 ? flags & ~0xf00u : flags;
}

constexpr bool ShouldUnlockBattleSwap(int target_fps) {
  return target_fps == 0 || target_fps == 60 || target_fps == 120;
}

constexpr bool ShouldUseGpuVsync(bool battle_active, int target_fps) {
  return !(battle_active && ShouldUnlockBattleSwap(target_fps));
}

constexpr bool ParticleWorkerSafeForTrailFree(std::uint32_t pending, bool allow_open_batch_park,
                                              bool worker_empty_parked, std::uint32_t ring_count) {
  if (pending == 0) return true;
  return allow_open_batch_park && worker_empty_parked && ring_count == 0;
}

inline bool InterpolateViewTranslation(const std::array<float, 16>& previous,
                                       std::array<float, 16>& current, float alpha) {
  if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) return false;
  for (unsigned i = 0; i < 12; ++i) {
    if (previous[i] != current[i]) return false;
  }
  for (unsigned i = 12; i < 16; ++i) {
    if (!std::isfinite(previous[i]) || !std::isfinite(current[i])) return false;
  }
  // With an unchanged basis and projection, translation is affine in the last row.
  for (unsigned i = 12; i < 16; ++i) current[i] = std::lerp(previous[i], current[i], alpha);
  return true;
}

using CameraMatrix = std::array<float, 16>;
using CameraAxis = std::array<float, 3>;

inline float Dot(CameraAxis a, CameraAxis b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline CameraAxis Cross(CameraAxis a, CameraAxis b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

inline CameraMatrix ViewProjection(const CameraMatrix& view, const CameraMatrix& projection) {
  CameraMatrix result{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 4; ++c)
      for (unsigned k = 0; k < 4; ++k) result[4 * r + c] += view[4 * r + k] * projection[4 * k + c];
  return result;
}

inline bool MatrixMatches(const CameraMatrix& a, const CameraMatrix& b) {
  for (unsigned i = 0; i < 16; ++i) {
    if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return false;
    if (std::abs(a[i] - b[i]) > .0001f * std::max(1.0f, std::abs(b[i]))) return false;
  }
  return true;
}

// Rigid view inverse matching sub_8222ADB8's +80 inverse-pose layout.
inline CameraMatrix InvertCameraPose(const CameraMatrix& pose) {
  CameraMatrix inv{};
  for (unsigned r = 0; r < 3; ++r) {
    for (unsigned c = 0; c < 3; ++c) inv[4 * r + c] = pose[4 * c + r];
    const CameraAxis axis{pose[4 * r], pose[4 * r + 1], pose[4 * r + 2]};
    const CameraAxis tr{pose[12], pose[13], pose[14]};
    inv[12 + r] = -Dot(axis, tr);
  }
  inv[15] = 1.0f;
  return inv;
}

inline bool InterpolateCameraPose(const CameraMatrix& previous, CameraMatrix& current, float alpha) {
  if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) return false;
  auto row = [](const CameraMatrix& m, unsigned r) -> CameraAxis {
    return {m[4 * r], m[4 * r + 1], m[4 * r + 2]};
  };
  auto rigid = [&](const CameraMatrix& m) {
    for (float value : m)
      if (!std::isfinite(value)) return false;
    const auto x = row(m, 0), y = row(m, 1), z = row(m, 2);
    return m[3] == 0 && m[7] == 0 && m[11] == 0 && m[15] == 1 &&
           std::abs(Dot(x, x) - 1) < .001f && std::abs(Dot(y, y) - 1) < .001f &&
           std::abs(Dot(z, z) - 1) < .001f && std::abs(Dot(x, y)) < .001f &&
           Dot(Cross(x, y), z) > .999f;
  };
  if (!rigid(previous) || !rigid(current)) return false;
  // Treat large jumps as camera cuts.
  for (unsigned r = 0; r < 3; ++r)
    if (Dot(row(previous, r), row(current, r)) < .5f) return false;
  if (alpha == 0) {
    current = previous;
    return true;
  }
  if (alpha == 1) return true;

  CameraAxis x{}, y{};
  for (unsigned i = 0; i < 3; ++i) {
    x[i] = std::lerp(previous[i], current[i], alpha);
    y[i] = std::lerp(previous[4 + i], current[4 + i], alpha);
  }
  const float x_length = std::sqrt(Dot(x, x));
  if (!(x_length > 0.0f)) return false;
  for (float& value : x) value /= x_length;
  const float along_x = Dot(x, y);
  for (unsigned i = 0; i < 3; ++i) y[i] -= along_x * x[i];
  const float y_length = std::sqrt(Dot(y, y));
  if (!(y_length > 0.0f)) return false;
  for (float& value : y) value /= y_length;
  const auto z = Cross(x, y);

  CameraAxis eye{};
  for (unsigned r = 0; r < 3; ++r)
    eye[r] = std::lerp(-Dot(row(previous, 3), row(previous, r)),
                       -Dot(row(current, 3), row(current, r)), alpha);
  for (unsigned i = 0; i < 3; ++i) {
    current[i] = x[i];
    current[4 + i] = y[i];
    current[8 + i] = z[i];
    current[12 + i] = -(eye[0] * x[i] + eye[1] * y[i] + eye[2] * z[i]);
  }
  return true;
}

struct FramePlan {
  unsigned ticks;
  double alpha;
  std::uint64_t discarded_ticks;
};

inline constexpr double kTurboBattleScale = 1.1;

constexpr bool PureActiveBattle(std::uint32_t f1416, std::uint32_t f1432, std::uint32_t f1492,
                                std::uint32_t f1504, std::uint32_t f2884, std::uint32_t f2904,
                                float f2856) {
  return f1416 == 0 && f1432 == 0 && f1492 == 0 && f1504 == 0 && f2884 == 0 && f2904 == 0 &&
         f2856 <= 0.0f;
}

constexpr double BattleStepNumerator(bool turbo_enabled, bool pure_active_battle) {
  return (turbo_enabled && pure_active_battle) ? kTurboBattleScale : 1.0;
}

class Fixed60 {
 public:
  // Store fractional ticks in billionths to avoid rounding 1/60 to nanoseconds.
  FramePlan advance(std::chrono::nanoseconds elapsed) {
    constexpr auto maximum = std::chrono::seconds(10);
    const auto bounded =
        std::clamp(elapsed, std::chrono::nanoseconds::zero(), std::chrono::nanoseconds(maximum));
    phase_ += static_cast<std::uint64_t>(bounded.count()) * 60;
    const auto due = phase_ / scale;
    phase_ %= scale;
    const auto ticks = std::min<std::uint64_t>(due, 4);
    return {static_cast<unsigned>(ticks), double(phase_) / scale, due - ticks};
  }

  double render_alpha(std::chrono::nanoseconds elapsed) const {
    const auto next =
        std::clamp(elapsed, std::chrono::nanoseconds::zero(), std::chrono::nanoseconds(16'666'667));
    return std::min(1.0, double(phase_ + static_cast<std::uint64_t>(next.count()) * 60) / scale);
  }

  void reset() { phase_ = 0; }

 private:
  static constexpr std::uint64_t scale = 1'000'000'000;
  std::uint64_t phase_ = 0;
};

}  // namespace dwg2::timing
