#pragma once
#include <atomic>
#include <cctype>
#include <cstdint>
#include <string>

namespace dwg2::prompts {

inline constexpr int kAuto = 0;
inline constexpr int kXbox360 = 1;
inline constexpr int kPs5 = 2;
inline constexpr int kKeyboard = 3;
inline constexpr int kXboxOne = 4;
inline constexpr int kXboxSeries = 5;
inline constexpr int kPs4 = 6;
inline constexpr int kPs3 = 7;
inline constexpr int kSwitch = 8;
inline constexpr int kMaxStyle = 8;

inline constexpr int kMenuOrder[9] = {0, 1, 4, 5, 7, 6, 2, 8, 3};

inline constexpr bool ValidChoice(int v) { return v >= 0 && v <= kMaxStyle; }
inline constexpr bool ValidStyle(int v) { return v >= 1 && v <= kMaxStyle; }
inline constexpr bool IsPlayStation(int style) {
  return style == kPs5 || style == kPs4 || style == kPs3;
}

inline constexpr const char* StyleName(int style) {
  switch (style) {
    case kAuto: return "Auto";
    case kXbox360: return "Xbox 360";
    case kPs5: return "PS5";
    case kKeyboard: return "Keyboard";
    case kXboxOne: return "Xbox One";
    case kXboxSeries: return "Xbox Series";
    case kPs4: return "PS4";
    case kPs3: return "PS3";
    case kSwitch: return "Switch";
    default: return "Xbox 360";
  }
}

// 0 Auto, 1..8 explicit prompt styles; last_device follows actual input, not presence.
inline std::atomic<int> choice{0}, last_device{kXbox360};
// 3 denotes keyboard activity. Controller style remains available for an
// explicit controller preference; Auto shows the active input device.
inline std::atomic<int> last_controller{kXbox360};
inline std::atomic<bool> hud_correction{true}, has_input_activity{false},
    controller_seeded{false};

inline int Effective() {
  const int c = choice.load();
  if (c != 0) return ValidStyle(c) ? c : kXbox360;
  const int dev = last_device.load();
  return ValidStyle(dev) ? dev : kXbox360;
}

inline void SeedConnectedController(int style) {
  if (ValidStyle(style) && style != kKeyboard && !has_input_activity.load() &&
      !controller_seeded.load() && !controller_seeded.exchange(true)) {
    last_device.store(style);
    last_controller.store(style);
  }
}

inline bool NotifyActivity(int style, bool is_controller) {
  if (!ValidStyle(style)) return false;
  has_input_activity.store(true);
  last_device.store(style);
  if (is_controller && style != kKeyboard) last_controller.store(style);
  return true;
}

inline int ClassifyGamepadStyle(int real_type, int type, uint16_t vendor,
                                uint16_t product, bool has_misc1,
                                const char* name) {
  const int eff = (real_type > 1) ? real_type : type;
  std::string lower;
  if (name) {
    for (const char* p = name; *p; ++p) {
      lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
  }
  const auto has = [&](const char* sub) {
    return lower.find(sub) != std::string::npos;
  };

  if (eff == 6 || (vendor == 0x054C && (product == 0x0CE6 || product == 0x0DF2)) ||
      has("dualsense") || has("ps5")) {
    return kPs5;
  }
  if (eff == 5 ||
      (vendor == 0x054C &&
       (product == 0x05C4 || product == 0x09CC || product == 0x0BA0 || product == 0x05C5)) ||
      has("dualshock 4") || has("ps4")) {
    return kPs4;
  }
  if (eff == 4 || (vendor == 0x054C && product == 0x0268) ||
      has("dualshock 3") || has("sixaxis") || has("ps3")) {
    return kPs3;
  }
  if ((eff >= 7 && eff <= 10) ||
      (vendor == 0x057E && (product == 0x2006 || product == 0x2007 || product == 0x2009 ||
                            product == 0x200E || product == 0x2017)) ||
      has("nintendo") || has("switch") || has("joy-con") || has("pro controller")) {
    return kSwitch;
  }
  if ((vendor == 0x045E && (product == 0x0B12 || product == 0x0B13)) ||
      (eff == 3 && has_misc1) || has("series")) {
    return kXboxSeries;
  }
  if (eff == 3 ||
      (vendor == 0x045E &&
       (product == 0x02D1 || product == 0x02DD || product == 0x02E0 || product == 0x02E3 ||
        product == 0x02EA || product == 0x02FD || product == 0x02FF || product == 0x0B00 ||
        product == 0x0B05 || product == 0x0B0A || product == 0x0B0C || product == 0x0B20 ||
        product == 0x0B21 || product == 0x0B22)) ||
      has("xbox one") || has("elite")) {
    return kXboxOne;
  }
  return kXbox360;
}

}  // namespace dwg2::prompts
