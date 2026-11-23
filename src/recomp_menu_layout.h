#pragma once

#include <string_view>

#include "recomp_menu_state.h"

namespace dwg2::recomp_menu {

struct Box {
  float x0, y0, x1, y1;

  float Width() const { return x1 - x0; }
  float Height() const { return y1 - y0; }
  Box Inset(float x, float y) const { return {x0 + x, y0 + y, x1 - x, y1 - y}; }
  Box Column(int index, int count, float gap) const {
    const float width = (Width() - gap * (count - 1)) / count;
    const float left = x0 + index * (width + gap);
    return {left, y0, left + width, y1};
  }
};

enum class Align { kLeft, kCenter, kRight };

struct TextPlacement {
  float x, y, scale;
};

inline TextPlacement PlaceText(Box box, float advance, float preferred_scale, Align align) {
  const float scale = std::min({preferred_scale, box.Height() / 28.0f,
                                box.Width() / std::max(1.0f, advance)});
  const float spare = box.Width() - advance * scale;
  const float offset = align == Align::kCenter ? spare / 2.0f : (align == Align::kRight ? spare : 0);
  return {box.x0 + offset, box.y0 + (box.Height() - 28.0f * scale) / 2.0f, scale};
}

constexpr uint32_t Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

template <typename Fill, typename Text, typename Hit>
void DrawMenuLayout(const MenuSnapshot& snap, float width, std::string_view fps_status,
                    Fill fill, Text text, Hit hit) {
  constexpr auto panel_color = Rgba(18, 17, 23, 224);
  constexpr auto line = Rgba(112, 94, 83);
  constexpr auto accent = Rgba(229, 193, 137);
  constexpr auto ink = Rgba(248, 240, 225);
  constexpr auto muted = Rgba(205, 193, 180);
  constexpr auto cell = Rgba(43, 38, 46);
  constexpr auto button = Rgba(56, 48, 55);
  constexpr auto focus = Rgba(79, 60, 57);
  constexpr auto selected_ink = Rgba(31, 23, 24);
  const bool wizard = snap.view == View::kControlsWizard;
  const bool modal = snap.view == View::kConfirmQuit || snap.view == View::kConfirmRestart;
  auto register_hit = [&](Box box, int id, int sub) {
    if (!modal || id == kHitQuitYes || id == kHitQuitNo) hit(box, id, sub);
  };

  auto framed = [&](Box box, uint32_t border, uint32_t color) {
    fill({box.x0, box.y0, box.x1, box.y0 + 1}, border);
    fill({box.x0, box.y1 - 1, box.x1, box.y1}, border);
    fill({box.x0, box.y0 + 1, box.x0 + 1, box.y1 - 1}, border);
    fill({box.x1 - 1, box.y0 + 1, box.x1, box.y1 - 1}, border);
    fill(box.Inset(1, 1), color);
  };
  auto label = [&](Box box, std::string_view value, float size = 0.81f,
                   uint32_t color = Rgba(248, 240, 225),
                   Align align = Align::kLeft) { text(box, value, size, color, align); };
  auto chip = [&](Box box, std::string_view value, int id, int sub = 0,
                  bool picked = false, bool focused = false) {
    framed(box, focused ? accent : line, picked ? accent : (focused ? focus : button));
    label(box.Inset(8, 3), value, 0.77f, picked ? selected_ink : ink, Align::kCenter);
    register_hit(box, id, sub);
  };
  auto row_background = [&](Box box, bool focused) {
    fill(box, focused ? focus : cell);
    if (focused) fill({box.x0, box.y0, box.x0 + 3, box.y1}, accent);
  };
  auto slider = [&](Box box, float fraction, int id) {
    const float mid = (box.y0 + box.y1) / 2;
    const float knob = box.x0 + fraction * box.Width();
    fill({box.x0, mid - 3, box.x1, mid + 3}, line);
    if (knob > box.x0) fill({box.x0, mid - 3, knob, mid + 3}, accent);
    fill({knob - kSliderThumbRadius, mid - 9, knob + kSliderThumbRadius, mid + 9}, ink);
    register_hit(box.Inset(-kSliderThumbRadius, 0), id, 0);
  };

  const float panel_width = std::min(width - 64, 1184.0f);
  const float edge = (width - panel_width) / 2;
  const Box panel{edge, 28, width - edge, 692};
  const float left = panel.x0 + 26, right = panel.x1 - 26;
  const float center = width / 2;
  fill({0, 0, width, 720}, Rgba(0, 0, 0, 70));
  framed(panel, line, panel_color);
  fill({left, 46, left + 42, 48}, accent);

  label({left, 54, center + 106, 96}, wizard ? "Controls Configuration" : "Recomp Menu",
        1.43f);
  const Box close_box{right - 148, 58, right, 90};
  const char* close_label = wizard ? "Back" : (snap.prompt_style == 2 ? "L2  Close"
      : (snap.prompt_style == 3 ? "F1  Close" : "LT  Close"));
  chip(close_box, close_label, wizard ? kHitWizBack : kHitClose);
  label({center + 108, 58, close_box.x0 - 14, 90}, fps_status, 0.70f, muted, Align::kRight);
  fill({left, 103, right, 104}, line);

  if (wizard) {
    const Box tabs{left + 44, 114, right - 44, 152};
    chip({left, tabs.y0, tabs.x0 - 8, tabs.y1}, "<", kHitWizPrevProfile);
    constexpr const char* profiles[] = {"Keyboard", "PS5", "Xbox"};
    for (int p = 0; p < 3; ++p) {
      chip(tabs.Column(p, 3, 8), profiles[p], kHitWizProfileTab, p, snap.wiz_profile == p);
    }
    chip({tabs.x1 + 8, tabs.y0, right, tabs.y1}, ">", kHitWizNextProfile);

    const Box hint{left, 160, right, 186};
    const bool capturing = snap.capturing_action >= 0;
    fill(hint, capturing ? focus : cell);
    label(hint.Inset(10, 1), capturing ? "Press new input. Esc or B cancels."
        : "Bind: Click / Enter / A     Clear: Del / X     Scroll: Wheel / Arrows / D-Pad",
        0.72f, capturing ? ink : muted, Align::kCenter);

    const auto& profile = snap.working_profiles[snap.wiz_profile];
    const Box table{left, 194, right - 16, 518};
    const float pitch = table.Height() / kWizVisibleRows;
    const float split = left + table.Width() * 0.42f;
    for (int row = 0; row < kWizVisibleRows; ++row) {
      const int action = snap.wiz_scroll + row;
      if (action >= dwg2::input::Count) break;
      const float y = table.y0 + row * pitch;
      const Box box{table.x0, y, table.x1, y + pitch - 4};
      const bool active = snap.capturing_action == action;
      row_background(box, snap.wiz_cursor == action || active);
      std::string name;
      for (wchar_t ch : std::wstring_view(dwg2::input::names[action])) {
        name += (ch >= 32 && ch <= 126) ? char(ch) : '?';
      }
      label({left + 12, box.y0, split - 10, box.y1}, name);
      std::string binding = active ? "Waiting for input..." :
          BindingLabel(profile.bind[action], snap.wiz_profile);
      if (!active && profile.alternate[action].kind != 0) {
        binding += " / " + BindingLabel(profile.alternate[action], snap.wiz_profile);
      }
      label({split + 10, box.y0, box.x1 - 12, box.y1}, binding, 0.80f, accent, Align::kRight);
      register_hit(box, kHitWizRow, action);
    }
    const float thumb_step = table.Height() / float(dwg2::input::Count);
    const float thumb_h = thumb_step * kWizVisibleRows;
    const float thumb_y = table.y0 + snap.wiz_scroll * thumb_step;
    fill({right - 6, table.y0, right, table.y1}, cell);
    fill({right - 6, thumb_y, right, thumb_y + thumb_h}, accent);

    const Box deadzone{left, 528, right, 570};
    row_background(deadzone, snap.wiz_cursor == kWizDeadzone);
    const float slider_left = left + deadzone.Width() * 0.28f;
    label({left + 12, deadzone.y0, slider_left - 12, deadzone.y1},
          "Deadzone: " + std::to_string(profile.deadzone) + "%");
    slider({slider_left, deadzone.y0, right - 16, deadzone.y1},
           std::clamp(profile.deadzone, 0, 50) / 50.0f, kHitWizDeadzone);
    const Box buttons{left, 580, right, 622};
    chip(buttons.Column(0, 3, 10), "Restore Defaults", kHitWizDefaults, 0, false,
         snap.wiz_cursor == kWizDefaults);
    chip(buttons.Column(1, 3, 10), "Save & Apply", kHitWizSave, 0, false,
         snap.wiz_cursor == kWizSave);
    chip(buttons.Column(2, 3, 10), "Back", kHitWizBack, 0, false, snap.wiz_cursor == kWizBack);
    label({left, 634, right, 674}, snap.toast, 0.76f, ink, Align::kCenter);
    return;
  }

  const float content = left + (right - left) * 0.155f;
  const float detail = content + (right - content) * 0.33f;
  const auto focused = [&](int item) { return !modal && snap.focus == item; };
  auto group = [&](float y, std::string_view name) {
    label({left, y, content - 12, y + 34}, name, 0.76f, accent);
  };
  float y = 116;
  auto next_row = [&](float height, float gap = 6.0f) {
    const Box box{content, y, right, y + height};
    y = box.y1 + gap;
    return box;
  };
  auto row = [&](int item, float height, float gap = 6.0f) {
    const Box box = next_row(height, gap);
    row_background(box, focused(item));
    register_hit(box, kHitMainRow, item);
    return box;
  };
  auto toggle = [&](int item, std::string_view name, bool enabled, float gap = 6.0f) {
    const Box box = row(item, 36, gap);
    const Box options{right - 160, box.y0 + 4, right - 6, box.y1 - 4};
    label({content + 12, box.y0, options.x0 - 10, box.y1}, name);
    chip(options.Column(0, 2, 6), "Off", kHitToggleOff, item, !enabled);
    chip(options.Column(1, 2, 6), "On", kHitToggleOn, item, enabled);
  };

  group(y, "File");
  const auto actions = next_row(40, 12);
  chip(actions.Column(0, 3, 8), "Restart", kHitMainRow, kRestart, false, focused(kRestart));
  chip(actions.Column(1, 3, 8), snap.paused ? "Resume" : "Pause",
       kHitMainRow, kPause, false, focused(kPause));
  chip(actions.Column(2, 3, 8), "Quit Game", kHitMainRow, kQuitGame, false, focused(kQuitGame));

  group(y, "Display");
  toggle(kFullscreen, "Fullscreen", snap.fullscreen);
  const Box resolution = row(kResolution, 38);
  const int res = snap.resolution_pending ? snap.resolution_pending : snap.resolution;
  label({content + 12, resolution.y0, detail - 8, resolution.y1},
        snap.resolution_pending ? "Applying resolution..." : "Resolution");
  const Box res_choices{detail, resolution.y0 + 4, right - 6, resolution.y1 - 4};
  constexpr const char* sizes[] = {"720p", "1440p", "2160p"};
  for (int i = 0; i < 3; ++i) {
    chip(res_choices.Column(i, 3, 6), sizes[i], kHitResStop, i + 1, res == i + 1);
  }

  const Box fov = row(kFov, 46, 12);
  const std::string fov_label = snap.fov == 0 ? "FOV: Default" :
      "FOV: +" + std::to_string(snap.fov) + "%";
  label({content + 12, fov.y0, detail - 8, fov.y1}, fov_label);
  slider({detail + 5, fov.y0 + 1, right - 12, fov.y0 + 26},
         std::clamp(snap.fov, 0, 100) / 100.0f, kHitFovSlider);
  const Box marks{detail, fov.y0 + 25, right - 8, fov.y1 - 1};
  label(marks.Column(0, 3, 4), "Default", 0.62f, muted);
  label(marks.Column(1, 3, 4), "+50% (Recommended)", 0.62f, muted, Align::kCenter);
  label(marks.Column(2, 3, 4), "+100%", 0.62f, muted, Align::kRight);

  group(y, "Performance");
  const Box fps = row(kFps, 38);
  label({content + 12, fps.y0, detail - 8, fps.y1}, "Frame Rate");
  const Box fps_choices{detail, fps.y0 + 4, right - 6, fps.y1 - 4};
  constexpr const char* rates[] = {"60 FPS", "120 FPS", "Uncapped"};
  constexpr int values[] = {60, 120, 0};
  for (int i = 0; i < 3; ++i) {
    chip(fps_choices.Column(i, 3, 6), rates[i], kHitFpsChoice, values[i], snap.fps == values[i]);
  }
  const auto note = next_row(28);
  label(note.Inset(12, 0), "Above 60 FPS is experimental. Game logic stays at 60 Hz.",
        0.72f, muted);
  toggle(kFpsCounter, "FPS Counter", snap.fps_counter);
  toggle(kVsync, "VSync", snap.vsync, 12);

  group(y, "Controls");
  const Box prompts = row(kButtonPrompts, 36);
  label({content + 12, prompts.y0, detail - 8, prompts.y1}, "Button Prompts");
  const Box prompt_choices{detail, prompts.y0 + 4, right - 6, prompts.y1 - 4};
  constexpr const char* styles[] = {"Xbox", "PS5", "Auto"};
  for (int i = 0; i < 3; ++i) {
    chip(prompt_choices.Column(i, 3, 6), styles[i], kHitPromptChoice, (i + 1) % 3,
         snap.prompt_choice == (i + 1) % 3);
  }
  const Box config = row(kConfigureControls, 38, 12);
  label({content + 12, config.y0, config.x1 - 42, config.y1}, "Configure Controls");
  label({config.x1 - 34, config.y0, config.x1 - 8, config.y1}, ">", 0.85f, accent,
        Align::kCenter);

  group(y, "About");
  const Box about = next_row(38);
  const Box updates{right - 232, about.y0, right, about.y1};
  label({content + 12, about.y0, updates.x0 - 12, about.y1},
        "Version: " + (snap.version_text.empty() ? std::string("Unknown") : snap.version_text),
        0.76f, muted);
  chip(updates, "Check for Updates", kHitMainRow, kCheckUpdates, false, focused(kCheckUpdates));

  fill({left, 624, right, 625}, line);
  label({left, 627, right, 650}, snap.toast, 0.76f, ink, Align::kCenter);
  label({left, 652, right, 676}, "D-Pad / Arrows: Navigate     A / Enter: Select     B / Esc: Close",
        0.72f, muted, Align::kCenter);

  if (modal) {
    fill({0, 0, width, 720}, Rgba(0, 0, 0, 146));
    const Box dialog{center - 248, 242, center + 248, 478};
    framed(dialog, accent, Rgba(30, 26, 32));
    label({dialog.x0 + 24, 264, dialog.x1 - 24, 310},
          snap.view == View::kConfirmRestart ? "Restart Game?" : "Quit Game?",
          1.32f, ink, Align::kCenter);
    label({dialog.x0 + 24, 320, dialog.x1 - 24, 360}, "Unsaved progress will be lost.",
          0.86f, muted, Align::kCenter);
    const Box buttons{dialog.x0 + 32, 394, dialog.x1 - 32, 446};
    chip(buttons.Column(0, 2, 12), "Yes", kHitQuitYes, 0, false, snap.confirm_quit_yes);
    chip(buttons.Column(1, 2, 12), "No", kHitQuitNo, 0, false, !snap.confirm_quit_yes);
  }
}

}  // namespace dwg2::recomp_menu
