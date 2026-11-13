#pragma once
#include <atomic>
namespace dwg2::prompts {
// 0 Auto, 1 Xbox, 2 PlayStation; last device follows actual input, not presence.
inline std::atomic<int> choice{0}, last_device{1};
// 3 denotes keyboard activity. Controller family remains available for an
// explicit Xbox/PlayStation preference; Auto shows the active input device.
inline std::atomic<int> last_controller{1};
inline std::atomic<bool> hud_correction{true};
inline int Effective() {
  if(last_device.load()==3)return 3;
  int value=choice.load();return value ? value : last_device.load();
}
}
