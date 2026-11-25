#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>
#include "player_settings.h"

namespace dwg2::ui_assets {

enum TexId : int {
  kTexFont = 0,
  kTexGlowBeam,
  kTexScrollBar,
  kTexDivider,
  kTexBackdrop,
  kTexFocusRing,
  kTexInnerCell,
  kTexOuterPanel,
  kTexWhite,
  kTexCount
};

struct Image {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;
};

struct GlyphMetric {
  uint16_t cell_x = 0, cell_y = 0;
  uint8_t min_x = 0, max_x = 0, advance = 10;
  bool valid = false;
};

struct AssetPack {
  std::array<Image, kTexCount> textures;
  std::array<GlyphMetric, 128> ascii_glyphs{};
  std::array<GlyphMetric, 128> latin1_glyphs{};
  GlyphMetric oe_upper{};
  GlyphMetric oe_lower{};
  bool from_game_data = false;

  const GlyphMetric* FindGlyph(uint32_t cp) const {
    if (cp < 128u) return ascii_glyphs[cp].valid ? &ascii_glyphs[cp] : nullptr;
    if (cp >= 0x80u && cp <= 0xFFu) {
      const auto& g = latin1_glyphs[cp - 0x80u];
      return g.valid ? &g : nullptr;
    }
    if (cp == 0x0152u) return oe_upper.valid ? &oe_upper : nullptr;
    if (cp == 0x0153u) return oe_lower.valid ? &oe_lower : nullptr;
    return nullptr;
  }

  bool HasGlyph(uint32_t cp) const { return FindGlyph(cp) != nullptr; }

  const GlyphMetric& GlyphForCodepoint(uint32_t cp) const {
    if (const auto* g = FindGlyph(cp)) return *g;
    return ascii_glyphs['?'];
  }
};

inline uint32_t NextUtf8Codepoint(std::string_view text, size_t& index) {
  if (index >= text.size()) return 0;
  const unsigned char b0 = static_cast<unsigned char>(text[index++]);
  if (b0 < 0x80u) return b0;
  auto cont = [&](uint32_t& out) -> bool {
    if (index >= text.size()) return false;
    const unsigned char bx = static_cast<unsigned char>(text[index]);
    if ((bx & 0xC0u) != 0x80u) return false;
    out = (out << 6) | (bx & 0x3Fu);
    ++index;
    return true;
  };
  uint32_t cp = 0xFFFDu;
  if ((b0 & 0xE0u) == 0xC0u && b0 >= 0xC2u) {
    cp = b0 & 0x1Fu;
    if (!cont(cp)) return 0xFFFDu;
  } else if ((b0 & 0xF0u) == 0xE0u) {
    cp = b0 & 0x0Fu;
    if (!cont(cp) || !cont(cp)) return 0xFFFDu;
    if (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu)) return 0xFFFDu;
  } else if ((b0 & 0xF8u) == 0xF0u && b0 <= 0xF4u) {
    cp = b0 & 0x07u;
    if (!cont(cp) || !cont(cp) || !cont(cp)) return 0xFFFDu;
    if (cp < 0x10000u || cp > 0x10FFFFu) return 0xFFFDu;
  }
  return cp;
}

namespace detail {

inline uint32_t ReadBE32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

struct FontCell { uint32_t cp; uint8_t row, col; };

template <typename Fn>
inline void ForEachFontCell(Fn&& fn) {
  // Texture 2 / 3 in Font.g1t (512x512): 16x10 grid of 28x28 cells.
  for (uint32_t cp = 0x21u; cp <= 0x7Eu; ++cp) {
    fn(cp, uint8_t((cp - 0x20u) / 16u), uint8_t((cp - 0x20u) % 16u));
  }
  static constexpr FontCell kExtended[] = {
      {0x00C4u, 6, 0},  {0x00D6u, 6, 1},  {0x00DCu, 6, 2},  {0x00E4u, 6, 3},
      {0x00F6u, 6, 4},  {0x00FCu, 6, 5},  {0x00DFu, 6, 6},  {0x00C7u, 6, 7},
      {0x00C9u, 6, 8},  {0x00E7u, 6, 9},  {0x00E0u, 6, 10}, {0x00E2u, 6, 11},
      {0x00E8u, 6, 12}, {0x00E9u, 6, 13}, {0x00EAu, 6, 14}, {0x00EBu, 6, 15},
      {0x00EEu, 7, 0},  {0x00EFu, 7, 1},  {0x00F4u, 7, 2},  {0x00FBu, 7, 3},
      {0x00F9u, 7, 4},  {0x0153u, 7, 5},  {0x00B0u, 7, 6},  {0x00C0u, 7, 7},
      {0x00C2u, 7, 8},  {0x0152u, 7, 9},  {0x00C8u, 7, 10}, {0x00CAu, 7, 11},
      {0x00CBu, 7, 12}, {0x00CEu, 7, 13}, {0x00CFu, 7, 14}, {0x00D4u, 7, 15},
      {0x00D9u, 8, 0},  {0x00DBu, 8, 1},  {0x00CCu, 8, 2},  {0x00ECu, 8, 3},
      {0x00D2u, 8, 4},  {0x00F2u, 8, 5},  {0x00D1u, 8, 6},  {0x00F1u, 8, 7},
      {0x00C1u, 8, 8},  {0x00E1u, 8, 9},  {0x00CDu, 8, 10}, {0x00EDu, 8, 11},
      {0x00D3u, 8, 12}, {0x00F3u, 8, 13}, {0x00DAu, 8, 14}, {0x00FAu, 8, 15},
      {0x00BFu, 9, 0},  {0x00A1u, 9, 1},  {0x00AEu, 9, 5},
  };
  for (const auto& e : kExtended) fn(e.cp, e.row, e.col);
}

inline GlyphMetric* MutGlyphSlot(AssetPack& pack, uint32_t cp) {
  if (cp < 128u) return &pack.ascii_glyphs[cp];
  if (cp >= 0x80u && cp <= 0xFFu) return &pack.latin1_glyphs[cp - 0x80u];
  if (cp == 0x0152u) return &pack.oe_upper;
  if (cp == 0x0153u) return &pack.oe_lower;
  return nullptr;
}

inline void MeasureFontGlyphs(const Image& font, AssetPack& pack) {
  pack.ascii_glyphs = {};
  pack.latin1_glyphs = {};
  pack.oe_upper = {};
  pack.oe_lower = {};
  pack.ascii_glyphs[' '].advance = 10;
  pack.ascii_glyphs[' '].valid = true;
  if (font.width == 0 || font.height == 0 || font.rgba.size() < size_t(font.width) * font.height * 4u)
    return;
  ForEachFontCell([&](uint32_t cp, uint8_t row, uint8_t col) {
    uint16_t cx = uint16_t(col) * 28u;
    uint16_t cy = uint16_t(row) * 28u;
    int min_x = 28, max_x = -1;
    for (int y = 0; y < 28; ++y) {
      if (uint32_t(cy + y) >= font.height) break;
      for (int x = 0; x < 28; ++x) {
        if (uint32_t(cx + x) >= font.width) break;
        uint8_t a = font.rgba[(size_t(cy + y) * font.width + (cx + x)) * 4u + 3u];
        if (a > 24) {
          if (x < min_x) min_x = x;
          if (x > max_x) max_x = x;
        }
      }
    }
    if (max_x >= min_x) {
      if (auto* g = MutGlyphSlot(pack, cp)) {
        g->cell_x = cx;
        g->cell_y = cy;
        g->min_x = uint8_t(min_x);
        g->max_x = uint8_t(max_x);
        int adv = (max_x - min_x + 1) + 2;
        if (adv < 6) adv = 6;
        if (adv > 28) adv = 28;
        g->advance = uint8_t(adv);
        g->valid = true;
      }
    }
  });
}

inline Image MakeSolid(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  Image img{w, h, std::vector<uint8_t>(size_t(w) * h * 4u)};
  for (size_t i = 0; i < size_t(w) * h; ++i) {
    img.rgba[i * 4 + 0] = r;
    img.rgba[i * 4 + 1] = g;
    img.rgba[i * 4 + 2] = b;
    img.rgba[i * 4 + 3] = a;
  }
  return img;
}

inline const uint8_t* FallbackGlyph5x7(uint32_t cp) {
  // 5-bit rows (bit 4 = left .. bit 0 = right), 7 rows high.
  struct Entry { uint32_t cp; uint8_t r[7]; };
  static constexpr Entry kFont[] = {
      {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
      {'2', {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}}, {'3', {0x1F,0x02,0x04,0x06,0x01,0x11,0x0E}},
      {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
      {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
      {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
      {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
      {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, {'D', {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
      {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
      {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
      {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
      {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
      {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
      {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
      {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
      {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
      {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
      {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}}, {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
      {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}}, {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
      {'a', {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}}, {'b', {0x10,0x10,0x16,0x19,0x11,0x11,0x1E}},
      {'c', {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E}}, {'d', {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F}},
      {'e', {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}}, {'f', {0x06,0x09,0x08,0x1C,0x08,0x08,0x08}},
      {'g', {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E}}, {'h', {0x10,0x10,0x16,0x19,0x11,0x11,0x11}},
      {'i', {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}}, {'j', {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}},
      {'k', {0x10,0x10,0x12,0x14,0x18,0x14,0x12}}, {'l', {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}},
      {'m', {0x00,0x00,0x1A,0x15,0x15,0x11,0x11}}, {'n', {0x00,0x00,0x16,0x19,0x11,0x11,0x11}},
      {'o', {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}}, {'p', {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}},
      {'q', {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01}}, {'r', {0x00,0x00,0x16,0x19,0x10,0x10,0x10}},
      {'s', {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E}}, {'t', {0x08,0x08,0x1C,0x08,0x08,0x09,0x06}},
      {'u', {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}}, {'v', {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}},
      {'w', {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}}, {'x', {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}},
      {'y', {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}}, {'z', {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}},
      {'_', {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}}, {',', {0x00,0x00,0x00,0x00,0x06,0x04,0x08}},
      {'.', {0x00,0x00,0x00,0x00,0x00,0x06,0x06}}, {':', {0x00,0x06,0x06,0x00,0x06,0x06,0x00}},
      {';', {0x00,0x06,0x06,0x00,0x06,0x04,0x08}}, {'$', {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}},
      {'^', {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}}, {'`', {0x08,0x04,0x02,0x00,0x00,0x00,0x00}},
      {'|', {0x04,0x04,0x04,0x00,0x04,0x04,0x04}}, {'?', {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}},
      {'!', {0x04,0x04,0x04,0x04,0x04,0x00,0x04}}, {'/', {0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
      {'\\',{0x10,0x10,0x08,0x04,0x02,0x01,0x01}}, {'~', {0x00,0x00,0x08,0x15,0x02,0x00,0x00}},
      {'\'',{0x04,0x04,0x08,0x00,0x00,0x00,0x00}}, {'"', {0x0A,0x0A,0x14,0x00,0x00,0x00,0x00}},
      {'(', {0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')', {0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
      {'[', {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}}, {']', {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},
      {'{', {0x02,0x04,0x04,0x08,0x04,0x04,0x02}}, {'}', {0x08,0x04,0x04,0x02,0x04,0x04,0x08}},
      {'+', {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}}, {'-', {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
      {'=', {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}}, {'<', {0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
      {'>', {0x08,0x04,0x02,0x01,0x02,0x04,0x08}}, {'%', {0x19,0x19,0x02,0x04,0x08,0x13,0x13}},
      {'#', {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}}, {'&', {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}},
      {'*', {0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00}}, {'@', {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}},
      {0x00C4u, {0x0A,0x04,0x0A,0x11,0x1F,0x11,0x11}}, {0x00D6u, {0x0A,0x00,0x0E,0x11,0x11,0x11,0x0E}},
      {0x00DCu, {0x0A,0x00,0x11,0x11,0x11,0x11,0x0E}}, {0x00E4u, {0x0A,0x00,0x0E,0x01,0x0F,0x11,0x0F}},
      {0x00F6u, {0x0A,0x00,0x0E,0x11,0x11,0x11,0x0E}}, {0x00FCu, {0x0A,0x00,0x11,0x11,0x11,0x13,0x0D}},
      {0x00DFu, {0x0E,0x11,0x11,0x1E,0x11,0x1E,0x10}}, {0x00C7u, {0x0E,0x11,0x10,0x10,0x11,0x0E,0x04}},
      {0x00C9u, {0x02,0x04,0x1F,0x10,0x1E,0x10,0x1F}}, {0x00E7u, {0x00,0x0E,0x10,0x10,0x11,0x0E,0x04}},
      {0x00E0u, {0x08,0x04,0x0E,0x01,0x0F,0x11,0x0F}}, {0x00E2u, {0x04,0x0A,0x0E,0x01,0x0F,0x11,0x0F}},
      {0x00E8u, {0x08,0x04,0x0E,0x11,0x1F,0x10,0x0E}}, {0x00E9u, {0x02,0x04,0x0E,0x11,0x1F,0x10,0x0E}},
      {0x00EAu, {0x04,0x0A,0x0E,0x11,0x1F,0x10,0x0E}}, {0x00EBu, {0x0A,0x00,0x0E,0x11,0x1F,0x10,0x0E}},
      {0x00EEu, {0x04,0x0A,0x0C,0x04,0x04,0x04,0x0E}}, {0x00EFu, {0x0A,0x00,0x0C,0x04,0x04,0x04,0x0E}},
      {0x00F4u, {0x04,0x0A,0x0E,0x11,0x11,0x11,0x0E}}, {0x00FBu, {0x04,0x0A,0x11,0x11,0x11,0x13,0x0D}},
      {0x00F9u, {0x08,0x04,0x11,0x11,0x11,0x13,0x0D}}, {0x0153u, {0x00,0x00,0x1A,0x15,0x1F,0x14,0x0E}},
      {0x00B0u, {0x06,0x09,0x09,0x06,0x00,0x00,0x00}}, {0x00C0u, {0x08,0x04,0x0E,0x11,0x1F,0x11,0x11}},
      {0x00C2u, {0x04,0x0A,0x0E,0x11,0x1F,0x11,0x11}}, {0x0152u, {0x0F,0x15,0x14,0x17,0x14,0x14,0x0F}},
      {0x00C8u, {0x08,0x04,0x1F,0x10,0x1E,0x10,0x1F}}, {0x00CAu, {0x04,0x0A,0x1F,0x10,0x1E,0x10,0x1F}},
      {0x00CBu, {0x0A,0x00,0x1F,0x10,0x1E,0x10,0x1F}}, {0x00CEu, {0x04,0x0A,0x0E,0x04,0x04,0x04,0x0E}},
      {0x00CFu, {0x0A,0x00,0x0E,0x04,0x04,0x04,0x0E}}, {0x00D4u, {0x04,0x0A,0x0E,0x11,0x11,0x11,0x0E}},
      {0x00D9u, {0x08,0x04,0x11,0x11,0x11,0x11,0x0E}}, {0x00DBu, {0x04,0x0A,0x11,0x11,0x11,0x11,0x0E}},
      {0x00CCu, {0x08,0x04,0x0E,0x04,0x04,0x04,0x0E}}, {0x00ECu, {0x08,0x04,0x0C,0x04,0x04,0x04,0x0E}},
      {0x00D2u, {0x08,0x04,0x0E,0x11,0x11,0x11,0x0E}}, {0x00F2u, {0x08,0x04,0x0E,0x11,0x11,0x11,0x0E}},
      {0x00D1u, {0x0D,0x12,0x11,0x19,0x15,0x13,0x11}}, {0x00F1u, {0x0D,0x12,0x00,0x16,0x19,0x11,0x11}},
      {0x00C1u, {0x02,0x04,0x0E,0x11,0x1F,0x11,0x11}}, {0x00E1u, {0x02,0x04,0x0E,0x01,0x0F,0x11,0x0F}},
      {0x00CDu, {0x02,0x04,0x0E,0x04,0x04,0x04,0x0E}}, {0x00EDu, {0x02,0x04,0x0C,0x04,0x04,0x04,0x0E}},
      {0x00D3u, {0x02,0x04,0x0E,0x11,0x11,0x11,0x0E}}, {0x00F3u, {0x02,0x04,0x0E,0x11,0x11,0x11,0x0E}},
      {0x00DAu, {0x02,0x04,0x11,0x11,0x11,0x11,0x0E}}, {0x00FAu, {0x02,0x04,0x11,0x11,0x11,0x13,0x0D}},
      {0x00BFu, {0x04,0x00,0x04,0x08,0x10,0x11,0x0E}}, {0x00A1u, {0x04,0x00,0x04,0x04,0x04,0x04,0x04}},
      {0x00AEu, {0x0E,0x11,0x1D,0x15,0x1D,0x11,0x0E}},
  };
  for (const auto& e : kFont) {
    if (e.cp == cp) return e.r;
  }
  return nullptr;
}

inline AssetPack BuildFallbackPack() {
  AssetPack pack;
  pack.from_game_data = false;
  pack.textures[kTexWhite] = MakeSolid(4, 4, 255, 255, 255, 255);

  // 512x512 fallback font sheet using the 16x10 28x28 cell layout of Font.g1t tex 2.
  Image font{512, 512, std::vector<uint8_t>(512u * 512u * 4u, 0)};
  ForEachFontCell([&](uint32_t cp, uint8_t row, uint8_t col) {
    const uint8_t* bits = FallbackGlyph5x7(cp);
    if (!bits) return;
    uint32_t cx = uint32_t(col) * 28u + 3u;
    uint32_t cy = uint32_t(row) * 28u + 4u;
    for (int gy = 0; gy < 7; ++gy) {
      for (int gx = 0; gx < 5; ++gx) {
        if ((bits[gy] >> (4 - gx)) & 1u) {
          for (int sy = 0; sy < 3; ++sy) {
            for (int sx = 0; sx < 3; ++sx) {
              size_t p = (size_t(cy + gy * 3 + sy) * 512u + (cx + gx * 3 + sx)) * 4u;
              font.rgba[p + 0] = 255;
              font.rgba[p + 1] = 255;
              font.rgba[p + 2] = 255;
              font.rgba[p + 3] = 255;
            }
          }
        }
      }
    }
  });
  pack.textures[kTexFont] = std::move(font);
  MeasureFontGlyphs(pack.textures[kTexFont], pack);

  auto make_box = [](uint32_t w, uint32_t h, uint32_t border,
                     uint8_t br, uint8_t bg, uint8_t bb, uint8_t ba,
                     uint8_t fr, uint8_t fg, uint8_t fb, uint8_t fa) {
    Image img{w, h, std::vector<uint8_t>(size_t(w) * h * 4u)};
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        bool edge = x < border || y < border || x + border >= w || y + border >= h;
        size_t p = (size_t(y) * w + x) * 4u;
        img.rgba[p + 0] = edge ? br : fr;
        img.rgba[p + 1] = edge ? bg : fg;
        img.rgba[p + 2] = edge ? bb : fb;
        img.rgba[p + 3] = edge ? ba : fa;
      }
    }
    return img;
  };

  // Procedural UI chrome fallbacks matching game asset dimensions.
  {
    Image beam{128, 256, std::vector<uint8_t>(128u * 256u * 4u)};
    for (uint32_t y = 0; y < 256; ++y) {
      float fy = 1.0f - (float(y >= 128 ? y - 128 : 127 - y) / 128.0f);
      for (uint32_t x = 0; x < 128; ++x) {
        float fx = 1.0f - (float(x >= 64 ? x - 64 : 63 - x) / 64.0f);
        uint8_t a = uint8_t(fx * fy * 200.0f);
        size_t p = (size_t(y) * 128u + x) * 4u;
        beam.rgba[p + 0] = 246;
        beam.rgba[p + 1] = 234;
        beam.rgba[p + 2] = 211;
        beam.rgba[p + 3] = a;
      }
    }
    pack.textures[kTexGlowBeam] = std::move(beam);
  }
  pack.textures[kTexScrollBar] = make_box(32, 256, 4, 228, 202, 166, 220, 120, 96, 80, 180);
  {
    Image div{64, 4, std::vector<uint8_t>(64u * 4u * 4u)};
    for (uint32_t y = 0; y < 4; ++y) {
      for (uint32_t x = 0; x < 64; ++x) {
        float fx = 1.0f - (float(x >= 32 ? x - 32 : 31 - x) / 32.0f);
        size_t p = (size_t(y) * 64u + x) * 4u;
        div.rgba[p + 0] = 228;
        div.rgba[p + 1] = 202;
        div.rgba[p + 2] = 166;
        div.rgba[p + 3] = (y == 1 || y == 2) ? uint8_t(fx * 220.0f) : 0;
      }
    }
    pack.textures[kTexDivider] = std::move(div);
  }
  pack.textures[kTexBackdrop] = MakeSolid(128, 64, 20, 12, 14, 180);
  pack.textures[kTexFocusRing] = make_box(64, 64, 4, 246, 234, 211, 240, 0, 0, 0, 0);
  pack.textures[kTexInnerCell] = make_box(32, 32, 2, 110, 88, 82, 220, 32, 20, 22, 180);
  pack.textures[kTexOuterPanel] = make_box(64, 64, 3, 228, 202, 166, 220, 24, 14, 16, 200);
  return pack;
}

}  // namespace detail

inline bool DecodeXboxBc3ToRgba(const uint8_t* src, size_t src_bytes, uint32_t w, uint32_t h,
                                std::vector<uint8_t>& out_rgba) {
  if (!src || w == 0 || h == 0) return false;
  uint64_t bx_count = (uint64_t(w) + 3u) / 4u;
  uint64_t by_count = (uint64_t(h) + 3u) / 4u;
  if (bx_count * by_count * 16ull > src_bytes) return false;
  out_rgba.assign(size_t(w) * size_t(h) * 4u, 0);

  auto rgb565 = [](uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint8_t r5 = uint8_t((c >> 11) & 31u);
    uint8_t g6 = uint8_t((c >> 5) & 63u);
    uint8_t b5 = uint8_t(c & 31u);
    r = uint8_t((r5 << 3) | (r5 >> 2));
    g = uint8_t((g6 << 2) | (g6 >> 4));
    b = uint8_t((b5 << 3) | (b5 >> 2));
  };

  for (uint32_t by = 0; by < by_count; ++by) {
    for (uint32_t bx = 0; bx < bx_count; ++bx) {
      const uint8_t* p = src + (size_t(by) * bx_count + bx) * 16u;
      uint8_t blk[16];
      for (int i = 0; i < 16; i += 2) {
        blk[i] = p[i + 1];
        blk[i + 1] = p[i];
      }

      uint8_t a[8];
      a[0] = blk[0];
      a[1] = blk[1];
      if (a[0] > a[1]) {
        for (int i = 1; i <= 6; ++i) a[i + 1] = uint8_t(((7 - i) * a[0] + i * a[1]) / 7);
      } else {
        for (int i = 1; i <= 4; ++i) a[i + 1] = uint8_t(((5 - i) * a[0] + i * a[1]) / 5);
        a[6] = 0;
        a[7] = 255;
      }
      uint64_t abits = 0;
      for (int i = 0; i < 6; ++i) abits |= uint64_t(blk[2 + i]) << (8 * i);

      uint16_t c0 = uint16_t(blk[8]) | (uint16_t(blk[9]) << 8);
      uint16_t c1 = uint16_t(blk[10]) | (uint16_t(blk[11]) << 8);
      uint8_t col[4][3]{};
      rgb565(c0, col[0][0], col[0][1], col[0][2]);
      rgb565(c1, col[1][0], col[1][1], col[1][2]);
      for (int k = 0; k < 3; ++k) {
        col[2][k] = uint8_t((2 * col[0][k] + col[1][k]) / 3);
        col[3][k] = uint8_t((col[0][k] + 2 * col[1][k]) / 3);
      }
      uint32_t cbits = uint32_t(blk[12]) | (uint32_t(blk[13]) << 8) |
                       (uint32_t(blk[14]) << 16) | (uint32_t(blk[15]) << 24);

      for (uint32_t py = 0; py < 4; ++py) {
        uint32_t y = by * 4 + py;
        if (y >= h) continue;
        for (uint32_t px = 0; px < 4; ++px) {
          uint32_t x = bx * 4 + px;
          if (x >= w) continue;
          uint32_t idx = py * 4 + px;
          uint32_t ci = (cbits >> (2 * idx)) & 3u;
          uint32_t ai = uint32_t((abits >> (3 * idx)) & 7u);
          size_t dst = (size_t(y) * w + x) * 4u;
          out_rgba[dst + 0] = col[ci][0];
          out_rgba[dst + 1] = col[ci][1];
          out_rgba[dst + 2] = col[ci][2];
          out_rgba[dst + 3] = a[ai];
        }
      }
    }
  }
  return true;
}

inline bool ParseLinkdataG1t(const std::vector<uint8_t>& entry_data, size_t g1t_offset,
                             uint32_t tex_index, Image& out_image) {
  if (g1t_offset > entry_data.size() || entry_data.size() - g1t_offset < 28u) return false;
  const uint8_t* base = entry_data.data() + g1t_offset;
  size_t avail_total = entry_data.size() - g1t_offset;
  if (std::memcmp(base, "G1TG0050", 8) != 0) return false;

  uint32_t total = detail::ReadBE32(base + 8);
  uint32_t head = detail::ReadBE32(base + 12);
  uint32_t n = detail::ReadBE32(base + 16);
  (void)detail::ReadBE32(base + 20);
  (void)detail::ReadBE32(base + 24);
  if (total < 28u || head < 28u || tex_index >= n) return false;
  if (uint64_t(head) + uint64_t(n) * 4ull > avail_total) return false;

  uint32_t rel = detail::ReadBE32(base + head + size_t(tex_index) * 4u);
  if (uint64_t(head) + uint64_t(rel) + 8ull > avail_total) return false;
  const uint8_t* tex = base + head + rel;
  uint8_t fmt = tex[1];
  uint8_t dim = tex[2];
  uint32_t w = 1u << (dim >> 4);
  uint32_t h = 1u << (dim & 15u);
  if (fmt != 8u) return false;

  size_t pix_off = size_t(head) + size_t(rel) + 8u;
  if (!DecodeXboxBc3ToRgba(entry_data.data() + g1t_offset + pix_off,
                           avail_total - pix_off, w, h, out_image.rgba)) {
    return false;
  }
  out_image.width = w;
  out_image.height = h;
  return true;
}

inline std::filesystem::path FindLinkdataBns() {
  std::error_code ec;
  auto primary = dwg2::settings::Path().parent_path() / L"game-data" / L"data" / L"LINKDATA.BNS";
  if (std::filesystem::is_regular_file(primary, ec)) return primary;
  wchar_t buf[32768]{};
  DWORD n = GetEnvironmentVariableW(L"DWG2_GAME_DATA_ROOT", buf, 32768);
  if (n > 0 && n < 32768) return std::filesystem::path(buf) / L"data" / L"LINKDATA.BNS";
  return primary;
}

inline AssetPack LoadPack(const std::filesystem::path& bns_path = FindLinkdataBns()) {
  std::error_code ec;
  uint64_t file_size = std::filesystem::file_size(bns_path, ec);
  if (ec || file_size < 16u) return detail::BuildFallbackPack();

  std::ifstream in(bns_path, std::ios::binary);
  if (!in) return detail::BuildFallbackPack();

  uint8_t hdr[16]{};
  in.read(reinterpret_cast<char*>(hdr), 16);
  if (!in) return detail::BuildFallbackPack();

  uint32_t count = detail::ReadBE32(hdr + 4);
  if (count <= 1249u || 16ull + uint64_t(count) * 8ull > file_size) {
    return detail::BuildFallbackPack();
  }

  auto read_entry = [&](uint32_t index, std::vector<uint8_t>& out) -> bool {
    uint8_t rec[8]{};
    in.seekg(std::streamoff(16ull + uint64_t(index) * 8ull), std::ios::beg);
    in.read(reinterpret_cast<char*>(rec), 8);
    if (!in) return false;
    uint32_t packed = detail::ReadBE32(rec);
    uint32_t size = detail::ReadBE32(rec + 4);
    uint64_t offset = uint64_t(packed & 0x1FFFFFu) << 11;
    if (size == 0 || size > 64u * 1024u * 1024u || offset + size > file_size) return false;
    out.resize(size);
    in.seekg(std::streamoff(offset), std::ios::beg);
    in.read(reinterpret_cast<char*>(out.data()), std::streamsize(size));
    return bool(in);
  };

  std::vector<uint8_t> entry1247, entry1249;
  if (!read_entry(1247, entry1247) || !read_entry(1249, entry1249)) {
    return detail::BuildFallbackPack();
  }

  AssetPack pack;
  if (!ParseLinkdataG1t(entry1247, 0, 2, pack.textures[kTexFont])) {
    return detail::BuildFallbackPack();
  }
  auto& font_rgba = pack.textures[kTexFont].rgba;
  for (size_t i = 0; i + 3 < font_rgba.size(); i += 4) {
    font_rgba[i + 0] = 255;
    font_rgba[i + 1] = 255;
    font_rgba[i + 2] = 255;
  }
  detail::MeasureFontGlyphs(pack.textures[kTexFont], pack);

  struct ChromeSpec { TexId id; size_t g1t_offset; };
  static constexpr ChromeSpec kChrome[] = {
      {kTexGlowBeam, 560},   {kTexScrollBar, 33376}, {kTexDivider, 41616},
      {kTexBackdrop, 41920}, {kTexFocusRing, 50160}, {kTexInnerCell, 54304},
      {kTexOuterPanel, 55376},
  };
  for (const auto& c : kChrome) {
    if (!ParseLinkdataG1t(entry1249, c.g1t_offset, 0, pack.textures[c.id])) {
      return detail::BuildFallbackPack();
    }
  }

  pack.textures[kTexWhite] = detail::MakeSolid(4, 4, 255, 255, 255, 255);
  pack.from_game_data = true;
  return pack;
}

}  // namespace dwg2::ui_assets
