// Original button prompt pack loader, resampler, BC3 encoder, and atlas composer.
// Contains no game pixels or stb_image implementation.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "keyboard_art.h"
#include "prompt_packs_data.h"

namespace dwg2::prompt_packs {

using PngDecodeFn = std::vector<uint8_t> (*)(const uint8_t* data, size_t size, int& out_width,
                                             int& out_height);

struct GamepadBindings {
  std::array<int, 25> kind{}, code{}, sign{};
};

struct DecodedImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
};

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

inline const char* PackFolder(int style) {
  switch (style) {
    case 2: return "ps5";
    case 3: return "keyboard";
    case 4: return "xbox_one";
    case 5: return "xbox_series";
    case 6: return "ps4";
    case 7: return "ps3";
    case 8: return "switch";
    default: return nullptr;
  }
}

inline const DecodedImage* LoadPackImage(PngDecodeFn decode, const std::filesystem::path& root,
                                         std::string_view rel_path) {
  if (!decode || rel_path.empty()) return nullptr;
  std::string key;
  if (!root.empty()) {
    key = root.generic_string();
    key.push_back('\n');
  }
  key.append(rel_path);

  static std::mutex cache_mutex;
  static std::unordered_map<std::string, DecodedImage> cache;
  {
    std::lock_guard lock(cache_mutex);
    if (auto it = cache.find(key); it != cache.end()) {
      return it->second.rgba.empty() ? nullptr : &it->second;
    }
  }

  DecodedImage img;
  if (!root.empty()) {
    std::error_code ec;
    const auto disk = root / "prompts" / std::filesystem::path(rel_path);
    if (std::filesystem::exists(disk, ec) && std::filesystem::is_regular_file(disk, ec)) {
      std::ifstream in(disk, std::ios::binary | std::ios::ate);
      if (in) {
        const auto sz = in.tellg();
        if (sz > 0) {
          std::vector<uint8_t> bytes(static_cast<size_t>(sz));
          in.seekg(0);
          if (in.read(reinterpret_cast<char*>(bytes.data()), sz)) {
            int w = 0, h = 0;
            auto rgba = decode(bytes.data(), bytes.size(), w, h);
            if (w > 0 && h > 0 && rgba.size() == size_t(w) * size_t(h) * 4) {
              img = DecodedImage{w, h, std::move(rgba)};
            }
          }
        }
      }
    }
  }

  if (img.rgba.empty()) {
    if (const EmbeddedAsset* asset = FindEmbeddedAsset(rel_path)) {
      int w = 0, h = 0;
      auto rgba = decode(kPackPngBlob + asset->offset, asset->size, w, h);
      if (w > 0 && h > 0 && rgba.size() == size_t(w) * size_t(h) * 4) {
        img = DecodedImage{w, h, std::move(rgba)};
      }
    }
  }

  std::lock_guard lock(cache_mutex);
  auto [it, _] = cache.try_emplace(std::move(key), std::move(img));
  return it->second.rgba.empty() ? nullptr : &it->second;
}

inline std::vector<uint8_t> ResampleRgbaPremul(const uint8_t* src, int src_w, int src_h, int dst_w,
                                               int dst_h) {
  if (!src || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return {};
  if (src_w == dst_w && src_h == dst_h) {
    return std::vector<uint8_t>(src, src + size_t(src_w) * size_t(src_h) * 4);
  }
  std::vector<uint8_t> out(size_t(dst_w) * size_t(dst_h) * 4, 0);
  const bool box_x = src_w >= dst_w;
  const bool box_y = src_h >= dst_h;
  for (int dy = 0; dy < dst_h; ++dy) {
    const double sy0 = double(dy) * src_h / dst_h;
    const double sy1 = double(dy + 1) * src_h / dst_h;
    const double cy = std::clamp((dy + 0.5) * src_h / dst_h - 0.5, 0.0, double(src_h - 1));
    const int iy0 = box_y ? std::clamp(int(std::floor(sy0)), 0, src_h - 1) : int(std::floor(cy));
    const int iy1 = box_y ? std::clamp(int(std::ceil(sy1)) - 1, iy0, src_h - 1)
                          : std::min(iy0 + 1, src_h - 1);
    const double fy = cy - iy0;
    for (int dx = 0; dx < dst_w; ++dx) {
      const double sx0 = double(dx) * src_w / dst_w;
      const double sx1 = double(dx + 1) * src_w / dst_w;
      const double cx = std::clamp((dx + 0.5) * src_w / dst_w - 0.5, 0.0, double(src_w - 1));
      const int ix0 = box_x ? std::clamp(int(std::floor(sx0)), 0, src_w - 1) : int(std::floor(cx));
      const int ix1 = box_x ? std::clamp(int(std::ceil(sx1)) - 1, ix0, src_w - 1)
                            : std::min(ix0 + 1, src_w - 1);
      const double fx = cx - ix0;

      double pr = 0.0, pg = 0.0, pb = 0.0, pa = 0.0, wsum = 0.0;
      for (int iy = iy0; iy <= iy1; ++iy) {
        const double wy = box_y ? (std::min(sy1, double(iy + 1)) - std::max(sy0, double(iy)))
                                : (iy0 == iy1 ? 1.0 : (iy == iy0 ? (1.0 - fy) : fy));
        if (wy <= 0.0) continue;
        for (int ix = ix0; ix <= ix1; ++ix) {
          const double wx = box_x ? (std::min(sx1, double(ix + 1)) - std::max(sx0, double(ix)))
                                  : (ix0 == ix1 ? 1.0 : (ix == ix0 ? (1.0 - fx) : fx));
          const double w = wx * wy;
          if (w <= 0.0) continue;
          const uint8_t* p = src + (size_t(iy) * size_t(src_w) + size_t(ix)) * 4;
          const double a = p[3] / 255.0;
          pr += (p[0] * a) * w;
          pg += (p[1] * a) * w;
          pb += (p[2] * a) * w;
          pa += p[3] * w;
          wsum += w;
        }
      }
      uint8_t* d = out.data() + (size_t(dy) * size_t(dst_w) + size_t(dx)) * 4;
      if (wsum <= 0.0 || pa <= 1e-6) {
        d[0] = d[1] = d[2] = d[3] = 0;
      } else {
        const int out_a = std::clamp(int(std::lround(pa / wsum)), 0, 255);
        if (out_a == 0) {
          d[0] = d[1] = d[2] = d[3] = 0;
        } else {
          d[0] = uint8_t(std::clamp(int(std::lround(pr * 255.0 / pa)), 0, 255));
          d[1] = uint8_t(std::clamp(int(std::lround(pg * 255.0 / pa)), 0, 255));
          d[2] = uint8_t(std::clamp(int(std::lround(pb * 255.0 / pa)), 0, 255));
          d[3] = uint8_t(out_a);
        }
      }
    }
  }
  return out;
}

inline void ClearRect(std::vector<uint8_t>& dst, int dst_w, int dst_h, Rect r) {
  if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 || r.x + r.w > dst_w || r.y + r.h > dst_h ||
      dst.size() != size_t(dst_w) * size_t(dst_h) * 4) {
    return;
  }
  for (int y = 0; y < r.h; ++y) {
    std::memset(dst.data() + (size_t(r.y + y) * size_t(dst_w) + size_t(r.x)) * 4, 0,
                size_t(r.w) * 4);
  }
}

inline bool ReplaceRect(std::vector<uint8_t>& dst, int dst_w, int dst_h, Rect r, const uint8_t* src,
                        int src_w, int src_h, bool preserve_aspect = true) {
  if (!src || src_w <= 0 || src_h <= 0 || r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 ||
      r.x + r.w > dst_w || r.y + r.h > dst_h ||
      dst.size() != size_t(dst_w) * size_t(dst_h) * 4) {
    return false;
  }
  int tw = r.w, th = r.h, ox = 0, oy = 0;
  if (preserve_aspect && std::abs(double(src_w) / src_h - double(r.w) / r.h) > 0.18) {
    ClearRect(dst, dst_w, dst_h, r);
    const double s = std::min(double(r.w) / src_w, double(r.h) / src_h);
    tw = std::clamp(int(std::lround(src_w * s)), 1, r.w);
    th = std::clamp(int(std::lround(src_h * s)), 1, r.h);
    ox = (r.w - tw) / 2;
    oy = (r.h - th) / 2;
  }
  const auto resampled = ResampleRgbaPremul(src, src_w, src_h, tw, th);
  if (resampled.size() != size_t(tw) * size_t(th) * 4) return false;
  for (int y = 0; y < th; ++y) {
    std::memcpy(dst.data() + (size_t(r.y + oy + y) * size_t(dst_w) + size_t(r.x + ox)) * 4,
                resampled.data() + size_t(y) * size_t(tw) * 4, size_t(tw) * 4);
  }
  return true;
}

inline std::vector<uint8_t> EncodeBc3(const uint8_t* rgba, int width, int height) {
  if (!rgba || width <= 0 || height <= 0 || (width % 4) != 0 || (height % 4) != 0) return {};
  std::vector<uint8_t> out(size_t(width) * size_t(height), 0);
  auto rgb565 = [](const unsigned* c) {
    return uint16_t(((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3));
  };
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      uint8_t* block = out.data() + (size_t(y / 4) * size_t(width / 4) + size_t(x / 4)) * 16;
      uint8_t min_a = 255, max_a = 0;
      unsigned low[3] = {255, 255, 255}, high[3] = {0, 0, 0};
      bool any_visible = false;
      for (unsigned i = 0; i < 16; ++i) {
        const uint8_t* p = rgba + (size_t(y + i / 4) * size_t(width) + size_t(x + i % 4)) * 4;
        min_a = std::min(min_a, p[3]);
        max_a = std::max(max_a, p[3]);
        if (p[3] > 0) {
          any_visible = true;
          for (unsigned c = 0; c < 3; ++c) {
            low[c] = std::min(low[c], unsigned(p[c]));
            high[c] = std::max(high[c], unsigned(p[c]));
          }
        }
      }

      block[0] = max_a;
      block[1] = min_a;
      uint64_t alpha_bits = 0;
      if (max_a > min_a) {
        int apal[8] = {max_a, min_a};
        for (int k = 0; k < 6; ++k) {
          apal[k + 2] = ((6 - k) * int(max_a) + (1 + k) * int(min_a) + 3) / 7;
        }
        for (unsigned i = 0; i < 16; ++i) {
          const int a = rgba[(size_t(y + i / 4) * size_t(width) + size_t(x + i % 4)) * 4 + 3];
          unsigned best = 0;
          int best_err = std::abs(a - apal[0]);
          for (unsigned k = 1; k < 8; ++k) {
            const int err = std::abs(a - apal[k]);
            if (err < best_err) {
              best_err = err;
              best = k;
            }
          }
          alpha_bits |= uint64_t(best) << (i * 3);
        }
      }
      for (unsigned i = 0; i < 6; ++i) block[2 + i] = uint8_t(alpha_bits >> (i * 8));

      if (!any_visible) {
        std::memset(block + 8, 0, 8);
        continue;
      }

      uint16_t c0 = rgb565(high), c1 = rgb565(low);
      if (c0 < c1) std::swap(c0, c1);
      block[8] = uint8_t(c0);
      block[9] = uint8_t(c0 >> 8);
      block[10] = uint8_t(c1);
      block[11] = uint8_t(c1 >> 8);

      uint32_t color_bits = 0;
      if (c0 > c1) {
        auto unpack = [](uint16_t c, int out_rgb[3]) {
          const int r5 = (c >> 11) & 31, g6 = (c >> 5) & 63, b5 = c & 31;
          out_rgb[0] = (r5 << 3) | (r5 >> 2);
          out_rgb[1] = (g6 << 2) | (g6 >> 4);
          out_rgb[2] = (b5 << 3) | (b5 >> 2);
        };
        int colors[4][3]{};
        unpack(c0, colors[0]);
        unpack(c1, colors[1]);
        for (int c = 0; c < 3; ++c) {
          colors[2][c] = (2 * colors[0][c] + colors[1][c] + 1) / 3;
          colors[3][c] = (colors[0][c] + 2 * colors[1][c] + 1) / 3;
        }
        for (unsigned i = 0; i < 16; ++i) {
          const uint8_t* p = rgba + (size_t(y + i / 4) * size_t(width) + size_t(x + i % 4)) * 4;
          if (p[3] == 0) continue;
          unsigned best = 0;
          int best_err = 0x7fffffff;
          for (unsigned k = 0; k < 4; ++k) {
            int err = 0;
            for (int c = 0; c < 3; ++c) {
              const int d = int(p[c]) - colors[k][c];
              err += d * d;
            }
            if (err < best_err) {
              best_err = err;
              best = k;
            }
          }
          color_bits |= best << (i * 2);
        }
      }
      for (unsigned i = 0; i < 4; ++i) block[12 + i] = uint8_t(color_bits >> (i * 8));
    }
  }
  return out;
}

inline const char* KeyboardKeyFilename(unsigned vk, bool& is_wide) {
  is_wide = false;
  if (vk >= '0' && vk <= '9') {
    static constexpr const char* kDigits[10] = {
        "key_0.png", "key_1.png", "key_2.png", "key_3.png", "key_4.png",
        "key_5.png", "key_6.png", "key_7.png", "key_8.png", "key_9.png"};
    return kDigits[vk - '0'];
  }
  if (vk >= 'A' && vk <= 'Z') {
    static constexpr const char* kLetters[26] = {
        "key_a.png", "key_b.png", "key_c.png", "key_d.png", "key_e.png", "key_f.png", "key_g.png",
        "key_h.png", "key_i.png", "key_j.png", "key_k.png", "key_l.png", "key_m.png", "key_n.png",
        "key_o.png", "key_p.png", "key_q.png", "key_r.png", "key_s.png", "key_t.png", "key_u.png",
        "key_v.png", "key_w.png", "key_x.png", "key_y.png", "key_z.png"};
    return kLetters[vk - 'A'];
  }
  if (vk >= VK_F1 && vk <= VK_F12) {
    static constexpr const char* kFn[12] = {
        "key_f1.png", "key_f2.png",  "key_f3.png",  "key_f4.png",  "key_f5.png",  "key_f6.png",
        "key_f7.png", "key_f8.png",  "key_f9.png",  "key_f10.png", "key_f11.png", "key_f12.png"};
    return kFn[vk - VK_F1];
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    static constexpr const char* kNum[10] = {
        "key_num0.png", "key_num1.png", "key_num2.png", "key_num3.png", "key_num4.png",
        "key_num5.png", "key_num6.png", "key_num7.png", "key_num8.png", "key_num9.png"};
    return kNum[vk - VK_NUMPAD0];
  }
  switch (vk) {
    case VK_SPACE: is_wide = true; return "key_space.png";
    case VK_BACK: is_wide = true; return "key_backspace.png";
    case VK_RETURN: is_wide = true; return "key_enter.png";
    case VK_ESCAPE: is_wide = true; return "key_esc.png";
    case VK_TAB: is_wide = true; return "key_tab.png";
    case VK_CAPITAL: is_wide = true; return "key_caps.png";
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT: is_wide = true; return "key_shift.png";
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL: is_wide = true; return "key_ctrl.png";
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU: is_wide = true; return "key_alt.png";
    case VK_LEFT: return "key_left.png";
    case VK_RIGHT: return "key_right.png";
    case VK_UP: return "key_up.png";
    case VK_DOWN: return "key_down.png";
    case VK_INSERT: return "key_insert.png";
    case VK_DELETE: return "key_delete.png";
    case VK_HOME: return "key_home.png";
    case VK_END: return "key_end.png";
    case VK_PRIOR: return "key_pgup.png";
    case VK_NEXT: return "key_pgdn.png";
    case VK_SNAPSHOT: return "key_printscreen.png";
    case VK_SCROLL: return "key_scrolllock.png";
    case VK_PAUSE: return "key_pause.png";
    case VK_LWIN:
    case VK_RWIN: return "key_win.png";
    case VK_APPS: return "key_menu.png";
    case VK_NUMLOCK: return "key_numlock.png";
    case VK_DIVIDE: return "key_num_divide.png";
    case VK_MULTIPLY: return "key_num_multiply.png";
    case VK_SUBTRACT: return "key_num_minus.png";
    case VK_ADD: return "key_num_plus.png";
    case VK_DECIMAL: return "key_num_decimal.png";
    case 0xBA: return "key_semicolon.png";
    case 0xBB: return "key_equals.png";
    case 0xBC: return "key_comma.png";
    case 0xBD: return "key_minus.png";
    case 0xBE: return "key_period.png";
    case 0xBF: return "key_slash.png";
    case 0xC0: return "key_grave.png";
    case 0xDB: return "key_lbracket.png";
    case 0xDC: return "key_backslash.png";
    case 0xDD: return "key_rbracket.png";
    case 0xDE: return "key_apostrophe.png";
    default: return nullptr;
  }
}

namespace detail {

struct ActionSlot {
  int action;
  Rect rect;
  const char* default_asset;
};

struct StaticSlot {
  Rect rect;
  const char* asset;
};

inline constexpr std::array<ActionSlot, 4> kKind2Slots = {{
    {0, {72, 72, 72, 72}, "combat_hud/xbox_a.png"},
    {1, {0, 72, 72, 72}, "combat_hud/xbox_b.png"},
    {2, {396, 0, 72, 72}, "combat_hud/xbox_x.png"},
    {3, {324, 0, 72, 72}, "combat_hud/xbox_y.png"},
}};

inline constexpr std::array<ActionSlot, 12> kKind3Slots = {{
    {0, {297, 249, 28, 27}, "menu_icons/a.png"},
    {1, {325, 249, 27, 27}, "menu_icons/b.png"},
    {2, {705, 165, 28, 27}, "menu_icons/x.png"},
    {3, {393, 177, 28, 27}, "menu_icons/y.png"},
    {4, {422, 207, 26, 21}, "menu_icons/lb.png"},
    {5, {918, 98, 26, 22}, "menu_icons/rb.png"},
    {6, {395, 205, 23, 25}, "menu_icons/lt.png"},
    {7, {848, 141, 20, 25}, "menu_icons/rt.png"},
    {8, {791, 141, 52, 26}, "menu_icons/back.png"},
    {9, {422, 177, 53, 27}, "menu_icons/start.png"},
    {10, {673, 201, 28, 27}, "menu_icons/ls.png"},
    {11, {297, 220, 28, 28}, "menu_icons/rs.png"},
}};

inline constexpr std::array<ActionSlot, 2> kKind4Slots = {{
    {4, {688, 170, 44, 27}, "menu_shoulders/lb_large.png"},
    {5, {148, 208, 44, 27}, "menu_shoulders/rb_large.png"},
}};

inline constexpr std::array<StaticSlot, 5> kKind3StaticSlots = {{
    {{325, 221, 27, 27}, "menu_icons/dpad_left_right.png"},
    {{497, 145, 28, 27}, "menu_icons/dpad_up_down.png"},
    {{986, 0, 26, 28}, "menu_icons/dpad_4way.png"},
    {{353, 221, 28, 27}, "menu_icons/analog_stick_1.png"},
    {{733, 165, 28, 27}, "menu_icons/analog_stick_2.png"},
}};

inline constexpr int kDefaultKind[12] = {2, 2, 2, 2, 2, 2, 3, 3, 2, 2, 2, 2};
inline constexpr int kDefaultCode[12] = {0, 1, 2, 3, 9, 10, 4, 5, 4, 6, 7, 8};

inline Rect NativeRectForGamepadInput(int atlas_kind, int bk, int bc) {
  if (atlas_kind == 2) {
    if (bk == 2 && bc >= 0 && bc <= 3) return kKind2Slots[bc].rect;
    return {};
  }
  if (atlas_kind == 3) {
    if (bk == 2) {
      if (bc >= 0 && bc <= 3) return kKind3Slots[bc].rect;
      if (bc == 4) return kKind3Slots[8].rect;
      if (bc == 6) return kKind3Slots[9].rect;
      if (bc == 7) return kKind3Slots[10].rect;
      if (bc == 8) return kKind3Slots[11].rect;
      if (bc == 9) return kKind3Slots[4].rect;
      if (bc == 10) return kKind3Slots[5].rect;
      if (bc >= 11 && bc <= 14) return kKind3StaticSlots[2].rect;  // dpad_4way
    } else if (bk == 3) {
      if (bc == 4) return kKind3Slots[6].rect;
      if (bc == 5) return kKind3Slots[7].rect;
      if (bc >= 0 && bc <= 3) return kKind3StaticSlots[3].rect;  // analog_stick_1
    }
    return {};
  }
  if (atlas_kind == 4) {
    if (bk == 2 && bc == 9) return kKind4Slots[0].rect;
    if (bk == 2 && bc == 10) return kKind4Slots[1].rect;
    return {};
  }
  return {};
}

inline const DecodedImage* LoadRemappedGamepadImage(PngDecodeFn decode,
                                                    const std::filesystem::path& root,
                                                    const char* pack, int bk, int bc,
                                                    bool combat_hud) {
  auto load = [&](std::string_view rel) -> const DecodedImage* {
    std::string path(pack);
    path.push_back('/');
    path.append(rel);
    return LoadPackImage(decode, root, path);
  };
  if (bk == 2) {
    switch (bc) {
      case 0: return load(combat_hud ? "combat_hud/xbox_a.png" : "menu_icons/a.png");
      case 1: return load(combat_hud ? "combat_hud/xbox_b.png" : "menu_icons/b.png");
      case 2: return load(combat_hud ? "combat_hud/xbox_x.png" : "menu_icons/x.png");
      case 3: return load(combat_hud ? "combat_hud/xbox_y.png" : "menu_icons/y.png");
      case 4: return load("remappable_72x72/back.png");
      case 6: return load("remappable_72x72/start.png");
      case 7: return load("remappable_72x72/ls.png");
      case 8: return load("remappable_72x72/rs.png");
      case 9: return load("remappable_72x72/lb.png");
      case 10: return load("remappable_72x72/rb.png");
      case 11: return load("remappable_72x72/dpad_up.png");
      case 12: return load("remappable_72x72/dpad_down.png");
      case 13: return load("remappable_72x72/dpad_left.png");
      case 14: return load("remappable_72x72/dpad_right.png");
      case 15: {
        if (const auto* img = load("remappable_72x72/capture.png")) return img;
        if (const auto* img = load("remappable_72x72/share.png")) return img;
        return load("remappable_72x72/back.png");
      }
      case 20: {
        if (const auto* img = load("remappable_72x72/touchpad.png")) return img;
        return load("remappable_72x72/back.png");
      }
      default: return nullptr;
    }
  }
  if (bk == 3) {
    switch (bc) {
      case 0:
      case 1: return load("remappable_72x72/ls.png");
      case 2:
      case 3: return load("remappable_72x72/rs.png");
      case 4: return load("remappable_72x72/lt.png");
      case 5: return load("remappable_72x72/rt.png");
      default: return nullptr;
    }
  }
  return nullptr;
}

inline bool StampActionSlots(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height,
                             int kind, int style, PngDecodeFn decode,
                             const std::filesystem::path& root,
                             const dwg2::keyboard::Bindings& kb_bindings,
                             const GamepadBindings& gp_bindings, const ActionSlot* slots,
                             size_t slot_count) {
  const int w = int(width), h = int(height);
  if (style == 3) {
    for (size_t i = 0; i < slot_count; ++i) {
      const auto& s = slots[i];
      const unsigned packed = kb_bindings[s.action];
      const unsigned vk = packed & 255u;
      const unsigned mods = (packed >> 8) & 7u;
      const DecodedImage* img = nullptr;
      if (mods == 0) {
        bool is_wide = false;
        if (const char* name = KeyboardKeyFilename(vk, is_wide)) {
          if (is_wide && s.rect.w >= 40 && s.rect.w > s.rect.h) {
            img = LoadPackImage(decode, root, std::string("keyboard/keys_wide_53x27/") + name);
          }
          if (!img) {
            img = LoadPackImage(decode, root, std::string("keyboard/keys_72x72/") + name);
          }
        }
      }
      if (img) {
        ReplaceRect(pixels, w, h, s.rect, img->rgba.data(), img->width, img->height);
      } else {
        auto keycap = dwg2::keyboard::Keycap(s.rect.w, s.rect.h, packed);
        ReplaceRect(pixels, w, h, s.rect, keycap.data(), s.rect.w, s.rect.h, false);
      }
    }
    return true;
  }

  bool gp_empty = true;
  for (int k : gp_bindings.kind) {
    if (k != 0) {
      gp_empty = false;
      break;
    }
  }

  if (style == 1) {
    if (gp_empty) return true;
    std::vector<uint8_t> original;
    for (size_t i = 0; i < slot_count; ++i) {
      const auto& s = slots[i];
      const int bk = gp_bindings.kind[s.action];
      const int bc = gp_bindings.code[s.action];
      if (bk == kDefaultKind[s.action] && bc == kDefaultCode[s.action]) continue;
      if (bk == 0) {
        ClearRect(pixels, w, h, s.rect);
        continue;
      }
      const Rect src_rect = NativeRectForGamepadInput(kind, bk, bc);
      if (src_rect.w > 0 && src_rect.h > 0) {
        if (original.empty()) original = pixels;
        std::vector<uint8_t> patch(size_t(src_rect.w) * size_t(src_rect.h) * 4);
        for (int row = 0; row < src_rect.h; ++row) {
          std::memcpy(
              patch.data() + size_t(row) * size_t(src_rect.w) * 4,
              original.data() + (size_t(src_rect.y + row) * width + size_t(src_rect.x)) * 4,
              size_t(src_rect.w) * 4);
        }
        ReplaceRect(pixels, w, h, s.rect, patch.data(), src_rect.w, src_rect.h);
      } else if (const auto* img =
                     LoadRemappedGamepadImage(decode, root, "xbox_one", bk, bc, kind == 2)) {
        ReplaceRect(pixels, w, h, s.rect, img->rgba.data(), img->width, img->height);
      }
    }
    return true;
  }

  const char* pack = PackFolder(style);
  if (!pack) return false;
  for (size_t i = 0; i < slot_count; ++i) {
    const auto& s = slots[i];
    const int bk = gp_empty ? kDefaultKind[s.action] : gp_bindings.kind[s.action];
    const int bc = gp_empty ? kDefaultCode[s.action] : gp_bindings.code[s.action];
    if (bk == 0) {
      ClearRect(pixels, w, h, s.rect);
    } else if (bk == kDefaultKind[s.action] && bc == kDefaultCode[s.action]) {
      if (const auto* img = LoadPackImage(decode, root, std::string(pack) + "/" + s.default_asset)) {
        ReplaceRect(pixels, w, h, s.rect, img->rgba.data(), img->width, img->height);
      }
    } else if (kind == 4 && bk == 2 && (bc == 9 || bc == 10)) {
      const char* shoulder = bc == 9 ? "menu_shoulders/lb_large.png" : "menu_shoulders/rb_large.png";
      const auto* img = LoadPackImage(decode, root, std::string(pack) + "/" + shoulder);
      if (!img) img = LoadRemappedGamepadImage(decode, root, pack, bk, bc, false);
      if (img) ReplaceRect(pixels, w, h, s.rect, img->rgba.data(), img->width, img->height);
    } else if (const auto* img = LoadRemappedGamepadImage(decode, root, pack, bk, bc, kind == 2)) {
      ReplaceRect(pixels, w, h, s.rect, img->rgba.data(), img->width, img->height);
    }
  }
  return true;
}

}  // namespace detail

inline bool ApplyControllerDiagram(int style, PngDecodeFn decode, const std::filesystem::path& root,
                                   const dwg2::keyboard::Bindings& kb_bindings,
                                   const dwg2::keyboard::Bindings& kb_alt,
                                   std::vector<uint8_t>& blocks) {
  if (blocks.size() != 131072) return false;
  if (style == 1) return true;
  if (style == 3) {
    static constexpr unsigned kDefaultKb[8] = {VK_SPACE, VK_BACK, 'L', 'P', '1', '3', 'Q', 'E'};
    bool is_default = true;
    for (int i = 0; i < 8; ++i) {
      if (kb_bindings[i] != kDefaultKb[i]) {
        is_default = false;
        break;
      }
    }
    if (!is_default) {
      const auto rgba = dwg2::keyboard::Diagram(kb_bindings, kb_alt);
      if (rgba.size() != 512u * 256u * 4u) return false;
      blocks = EncodeBc3(rgba.data(), 512, 256);
      return blocks.size() == 131072;
    }
  }

  const char* pack = PackFolder(style);
  if (!pack) return false;
  const std::string rel = std::string(pack) + "/options_controller/controller_diagram.png";
  std::string cache_key;
  if (!root.empty()) {
    cache_key = root.generic_string();
    cache_key.push_back('\n');
  }
  cache_key.append(rel);

  static std::mutex bc3_mutex;
  static std::unordered_map<std::string, std::vector<uint8_t>> bc3_cache;
  {
    std::lock_guard lock(bc3_mutex);
    if (auto it = bc3_cache.find(cache_key); it != bc3_cache.end()) {
      if (it->second.size() != 131072) return false;
      blocks = it->second;
      return true;
    }
  }

  const DecodedImage* img = LoadPackImage(decode, root, rel);
  if (!img || img->width != 512 || img->height != 256) return false;
  auto encoded = EncodeBc3(img->rgba.data(), 512, 256);
  if (encoded.size() != 131072) return false;

  std::lock_guard lock(bc3_mutex);
  auto [it, _] = bc3_cache.try_emplace(std::move(cache_key), std::move(encoded));
  blocks = it->second;
  return true;
}

inline bool ApplyPromptAtlas(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height, int kind,
                             int style, PngDecodeFn decode, const std::filesystem::path& root,
                             const dwg2::keyboard::Bindings& kb_bindings,
                             const dwg2::keyboard::Bindings& /*kb_alt*/,
                             const GamepadBindings& gp_bindings) {
  if (width == 0 || height == 0 || pixels.size() != size_t(width) * size_t(height) * 4) {
    return false;
  }
  if (kind == 2 && width == 512 && height == 256) {
    return detail::StampActionSlots(pixels, width, height, kind, style, decode, root, kb_bindings,
                                    gp_bindings, detail::kKind2Slots.data(),
                                    detail::kKind2Slots.size());
  }
  if (kind == 3 && width == 1024 && height == 512) {
    if (!detail::StampActionSlots(pixels, width, height, kind, style, decode, root, kb_bindings,
                                  gp_bindings, detail::kKind3Slots.data(),
                                  detail::kKind3Slots.size())) {
      return false;
    }
    if (const char* pack = PackFolder(style)) {
      for (const auto& s : detail::kKind3StaticSlots) {
        if (const auto* img = LoadPackImage(decode, root, std::string(pack) + "/" + s.asset)) {
          ReplaceRect(pixels, int(width), int(height), s.rect, img->rgba.data(), img->width,
                      img->height);
        }
      }
    }
    return true;
  }
  if (kind == 4 && width == 1024 && height == 512) {
    return detail::StampActionSlots(pixels, width, height, kind, style, decode, root, kb_bindings,
                                    gp_bindings, detail::kKind4Slots.data(),
                                    detail::kKind4Slots.size());
  }
  return false;
}

}  // namespace dwg2::prompt_packs
