#pragma once
#include <atomic>
namespace dwg2::prompts {
// 0 Auto, 1 Xbox, 2 PlayStation; last device follows actual input, not presence.
inline std::atomic<int> choice{0}, last_device{1};
// Keyboard activity changes Auto prompts, but never changes the controller image.
inline std::atomic<int> last_controller{1};
inline std::atomic<bool> hud_correction{true};
inline int Effective() {
  int value=choice.load();return value ? value : last_device.load();
}
}
