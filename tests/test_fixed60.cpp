#include "fixed60.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace std::chrono;
using dwg2::timing::Fixed60;

int main() {
  std::array<float, 16> previous{};
  previous[0] = previous[5] = previous[10] = previous[15] = 1;
  auto current = previous;
  current[12] = 10;
  assert(dwg2::timing::InterpolateViewTranslation(previous, current, .25f));
  assert(current[12] == 2.5f && current[0] == 1 && current[15] == 1);
  auto turn = previous;
  turn[0] = turn[5] = .8f;
  turn[1] = .6f;
  turn[4] = -.6f;
  turn[12] = -8;
  turn[13] = -6;
  assert(dwg2::timing::InterpolateCameraPose(previous, turn, .5f));
  assert(std::abs(turn[0] - .9486833f) < .00001f);
  assert(std::abs(turn[1] - .3162278f) < .00001f);
  assert(std::abs(turn[12] + 5 * turn[0]) < .00001f);
  auto projection = previous;
  projection[0] = 2;
  projection[5] = 3;
  const auto combined = dwg2::timing::ViewProjection(turn, projection);
  assert(combined[1] == 3 * turn[1] && combined[12] == 2 * turn[12]);
  assert(dwg2::timing::MatrixMatches(combined, combined));
  const auto inv_turn = dwg2::timing::InvertCameraPose(turn);
  const auto roundtrip = dwg2::timing::ViewProjection(turn, inv_turn);
  assert(dwg2::timing::MatrixMatches(roundtrip, previous));
  assert(dwg2::timing::ShouldUnlockBattleSwap(0));
  assert(dwg2::timing::ShouldUnlockBattleSwap(120));
  assert(dwg2::timing::ShouldUnlockBattleSwap(60));
  assert(!dwg2::timing::ShouldUnlockBattleSwap(30));
  assert(dwg2::timing::ShouldUseGpuVsync(false, 0));
  assert(dwg2::timing::ShouldUseGpuVsync(false, 120));
  assert(dwg2::timing::ShouldUseGpuVsync(false, 60));
  assert(dwg2::timing::ShouldUseGpuVsync(false, 30));
  assert(!dwg2::timing::ShouldUseGpuVsync(true, 0));
  assert(!dwg2::timing::ShouldUseGpuVsync(true, 120));
  assert(!dwg2::timing::ShouldUseGpuVsync(true, 60));
  assert(dwg2::timing::ShouldUseGpuVsync(true, 30));
  assert(dwg2::timing::ParticleWorkerSafeForTrailFree(0, false, false, 3));
  assert(dwg2::timing::ParticleWorkerSafeForTrailFree(0, true, false, 0));
  assert(dwg2::timing::ParticleWorkerSafeForTrailFree(1, true, true, 0));
  assert(!dwg2::timing::ParticleWorkerSafeForTrailFree(1, true, false, 0));
  assert(!dwg2::timing::ParticleWorkerSafeForTrailFree(1, true, true, 1));
  assert(!dwg2::timing::ParticleWorkerSafeForTrailFree(1, false, true, 0));
  auto cut = previous;
  cut[0] = cut[5] = -1;
  assert(!dwg2::timing::InterpolateCameraPose(previous, cut, .5f) && cut[0] == -1);
  auto opposed_y = previous;
  opposed_y[4] = 0;
  opposed_y[5] = -1;
  opposed_y[10] = -1;
  assert(!dwg2::timing::InterpolateCameraPose(previous, opposed_y, .5f));
  current[0] = 2;
  const auto rotated = current;
  assert(!dwg2::timing::InterpolateViewTranslation(previous, current, .5f));
  assert(current == rotated);
  for (const std::uint32_t flags : {0u, 0xa5a5505au, 0xfffff0ffu}) {
    for (std::uint32_t interval = 0; interval < 16; ++interval) {
      const auto packed = flags | (interval << 8);
      const auto expected = interval >= 1 && interval <= 3 ? flags : packed;
      assert(dwg2::timing::ImmediateBattleSwap(packed) == expected);
    }
  }
  for (int rate : {30, 60, 75, 120, 144, 240}) {
    Fixed60 clock;
    long long prior = 0;
    unsigned ticks = 0;
    double prev_val = 0, curr_val = 0, last = 0;
    for (long long frame = 1; frame <= rate * 60; ++frame) {
      const auto next = frame * 1'000'000'000 / rate;
      const auto step = clock.advance(nanoseconds(next - prior));
      prior = next;
      assert(step.discarded_ticks == 0);
      assert(step.alpha >= 0 && step.alpha < 1);
      for (unsigned i = 0; i < step.ticks; ++i) {
        prev_val = curr_val;
        ++curr_val;
        ++ticks;
      }
      const auto visual = std::lerp(prev_val, curr_val, step.alpha);
      assert(visual >= last);
      last = visual;
    }
    assert(ticks == 3600 && last == 3599);
    std::cout << rate << " render samples/s: " << ticks << " simulation ticks in 60 s\n";
  }
  Fixed60 clock;
  assert(clock.advance(-1ms).ticks == 0);
  const auto stalled = clock.advance(200ms);
  assert(stalled.ticks == 4 && stalled.discarded_ticks == 8 && stalled.alpha == 0);
  assert(clock.advance(8ms).alpha > 0);
  clock.reset();
  assert(clock.advance(0ns).alpha == 0);
  assert(clock.advance(5ms).alpha == 0.3);
  assert(clock.render_alpha(-1ms) == 0.3);
  assert(clock.render_alpha(5ms) == 0.6);
  assert(clock.render_alpha(50ms) == 1.0);
  assert(clock.advance(5ms).alpha == 0.6);
}
