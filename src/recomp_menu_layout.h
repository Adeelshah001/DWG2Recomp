#pragma once

#include <string_view>

#include "recomp_menu_l10n.h"
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

template <typename Fill, typename Tri, typename Text, typename Hit>
void DrawMenuLayout(const MenuSnapshot& snap, float width, std::string_view fps_status,
                    Fill fill, Tri tri, Text text, Hit hit) {
  constexpr auto panel_color = Rgba(18, 17, 23, 224);
  constexpr auto line = Rgba(112, 94, 83);
  constexpr auto accent = Rgba(229, 193, 137);
  constexpr auto ink = Rgba(248, 240, 225);
  constexpr auto muted = Rgba(205, 193, 180);
  constexpr auto cell = Rgba(43, 38, 46);
  constexpr auto button = Rgba(56, 48, 55);
  constexpr auto focus = Rgba(79, 60, 57);
  constexpr auto selected_ink = Rgba(31, 23, 24);
  const auto& t = l10n::Get(snap.language);
  const bool wizard = snap.view == View::kControlsWizard;
  const bool modal = snap.view == View::kConfirmQuit || snap.view == View::kConfirmRestart ||
                     snap.view == View::kConfirmLanguageRestart;
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
  fill({left, 44, left + 42, 46}, accent);

  label({left, 52, center + 106, 93}, wizard ? t.title_wizard : t.title_main, 1.43f);
  const Box close_box{right - 148, 56, right, 88};
  const std::string close_label =
      wizard ? std::string(t.back) : l10n::FormatClose(snap.language, snap.prompt_style);
  chip(close_box, close_label, wizard ? kHitWizBack : kHitClose);
  label({center + 108, 56, close_box.x0 - 14, 88}, fps_status, 0.70f, muted, Align::kRight);
  fill({left, 100, right, 101}, line);

  if (wizard) {
    const Box tabs{left + 44, 112, right - 44, 150};
    chip({left, tabs.y0, tabs.x0 - 8, tabs.y1}, "<", kHitWizPrevProfile);
    const char* profiles[3] = {t.wiz_keyboard, "PS", "Xbox"};
    for (int p = 0; p < 3; ++p) {
      chip(tabs.Column(p, 3, 8), profiles[p], kHitWizProfileTab, p, snap.wiz_profile == p);
    }
    chip({tabs.x1 + 8, tabs.y0, right, tabs.y1}, ">", kHitWizNextProfile);

    const Box hint{left, 158, right, 184};
    const bool capturing = snap.capturing_action >= 0;
    fill(hint, capturing ? focus : cell);
    label(hint.Inset(10, 1), capturing ? t.wiz_press_input : t.wiz_hint,
          0.72f, capturing ? ink : muted, Align::kCenter);

    const auto& profile = snap.working_profiles[snap.wiz_profile];
    const Box table{left, 192, right - 16, 516};
    const float pitch = table.Height() / kWizVisibleRows;
    const float split = left + table.Width() * 0.42f;
    for (int row = 0; row < kWizVisibleRows; ++row) {
      const int action = snap.wiz_scroll + row;
      if (action >= dwg2::input::Count) break;
      const float y = table.y0 + row * pitch;
      const Box box{table.x0, y, table.x1, y + pitch - 4};
      const bool active = snap.capturing_action == action;
      row_background(box, snap.wiz_cursor == action || active);
      label({left + 12, box.y0, split - 10, box.y1}, t.actions[action]);
      std::string binding = active
                                ? std::string(t.wiz_waiting)
                                : BindingLabel(profile.bind[action], snap.wiz_profile, snap.language);
      if (!active && profile.alternate[action].kind != 0) {
        binding += " / " + BindingLabel(profile.alternate[action], snap.wiz_profile, snap.language);
      }
      label({split + 10, box.y0, box.x1 - 12, box.y1}, binding, 0.80f, accent, Align::kRight);
      register_hit(box, kHitWizRow, action);
    }
    const float thumb_step = table.Height() / float(dwg2::input::Count);
    const float thumb_h = thumb_step * kWizVisibleRows;
    const float thumb_y = table.y0 + snap.wiz_scroll * thumb_step;
    fill({right - 6, table.y0, right, table.y1}, cell);
    fill({right - 6, thumb_y, right, thumb_y + thumb_h}, accent);

    const Box deadzone{left, 526, right, 568};
    row_background(deadzone, snap.wiz_cursor == kWizDeadzone);
    const float slider_left = left + deadzone.Width() * 0.28f;
    label({left + 12, deadzone.y0, slider_left - 12, deadzone.y1},
          l10n::FormatDeadzone(snap.language, profile.deadzone));
    slider({slider_left, deadzone.y0, right - 16, deadzone.y1},
           std::clamp(profile.deadzone, 0, 50) / 50.0f, kHitWizDeadzone);
    const Box buttons{left, 578, right, 620};
    chip(buttons.Column(0, 3, 10), t.wiz_defaults, kHitWizDefaults, 0, false,
         snap.wiz_cursor == kWizDefaults);
    chip(buttons.Column(1, 3, 10), t.wiz_save, kHitWizSave, 0, false,
         snap.wiz_cursor == kWizSave);
    chip(buttons.Column(2, 3, 10), t.back, kHitWizBack, 0, false, snap.wiz_cursor == kWizBack);
    label({left, 632, right, 672}, snap.toast, 0.76f, ink, Align::kCenter);
    return;
  }

  const float content = left + (right - left) * 0.155f;
  const float detail = content + (right - content) * 0.33f;
  const auto focused = [&](int item) { return !modal && snap.focus == item; };
  auto group = [&](float gy, std::string_view name) {
    label({left, gy + 4, content - 12, gy + 34}, name, 0.76f, accent);
  };
  const int start_row = std::clamp(snap.main_scroll, 0, MaxMainScroll());
  const int last_row = LastVisibleMainRow(start_row);
  const bool can_scroll_up = start_row > 0;
  const bool can_scroll_down = last_row < kMainRowCount - 1;
  float y = kMainViewTop;
  auto next_row = [&](float height, float gap) {
    const Box box{content, y, right, y + height};
    y = box.y1 + gap;
    return box;
  };
  auto row = [&](int item, float height, float gap) {
    const Box box = next_row(height, gap);
    row_background(box, focused(item));
    register_hit(box, kHitMainRow, item);
    return box;
  };
  auto toggle = [&](int item, std::string_view name, bool enabled, float gap) {
    const Box box = row(item, 38, gap);
    const Box options{right - 160, box.y0 + 5, right - 6, box.y1 - 5};
    label({content + 12, box.y0 + 4, options.x0 - 10, box.y1 - 4}, name);
    chip(options.Column(0, 2, 6), t.off, kHitToggleOff, item, !enabled);
    chip(options.Column(1, 2, 6), t.on, kHitToggleOn, item, enabled);
  };
  auto cycle_row = [&](int item, std::string_view name, std::string_view val, int hit_id,
                       float gap, bool register_center = false) {
    const Box box = row(item, 39, gap);
    label({content + 12, box.y0 + 4, detail - 8, box.y1 - 4}, name);
    const Box choices{detail, box.y0 + 5, right - 6, box.y1 - 5};
    constexpr float kArrowWidth = 42.0f;
    chip({choices.x0, choices.y0, choices.x0 + kArrowWidth, choices.y1}, "<", hit_id, -1);
    const Box val_box{choices.x0 + kArrowWidth + 6.0f, choices.y0,
                      choices.x1 - kArrowWidth - 6.0f, choices.y1};
    framed(val_box, line, button);
    label(val_box.Inset(8, 3), val, 0.77f, ink, Align::kCenter);
    if (register_center) register_hit(val_box, hit_id, 0);
    chip({choices.x1 - kArrowWidth, choices.y0, choices.x1, choices.y1}, ">", hit_id, -2);
  };

  const char* group_labels[6] = {
      t.grp_file, t.grp_display, t.grp_performance, t.grp_controls, t.grp_language, t.grp_about};
  auto row_group = [](int r) {
    if (r == 0) return 0;
    if (r <= 3) return 1;
    if (r <= 7) return 2;
    if (r <= 9) return 3;
    return r == 10 ? 4 : 5;
  };
  int last_group = -1;
  for (int r = start_row; r <= last_row; ++r) {
    const float h = MainRowHeight(r);
    const float gap = MainRowGap(r);
    if (y + h > kMainViewBottom) break;
    const int g = row_group(r);
    if (g != last_group) {
      group(y, group_labels[g]);
      last_group = g;
    }
    switch (r) {
      case 0: {
        const auto actions = next_row(h, gap);
        chip(actions.Column(0, 3, 8), snap.paused ? t.resume : t.pause,
             kHitMainRow, kPause, false, focused(kPause));
        chip(actions.Column(1, 3, 8), t.restart, kHitMainRow, kRestart, false, focused(kRestart));
        chip(actions.Column(2, 3, 8), t.quit_game, kHitMainRow, kQuitGame, false,
             focused(kQuitGame));
        break;
      }
      case 1:
        toggle(kFullscreen, t.fullscreen, snap.fullscreen, gap);
        break;
      case 2: {
        const Box resolution = row(kResolution, h, gap);
        const int res = snap.resolution_pending ? snap.resolution_pending : snap.resolution;
        label({content + 12, resolution.y0 + 4, detail - 8, resolution.y1 - 4},
              snap.resolution_pending ? t.applying_res : t.resolution);
        const Box res_choices{detail, resolution.y0 + 5, right - 6, resolution.y1 - 5};
        constexpr const char* sizes[] = {"720p", "1440p", "2160p"};
        for (int i = 0; i < 3; ++i) {
          chip(res_choices.Column(i, 3, 6), sizes[i], kHitResStop, i + 1, res == i + 1);
        }
        break;
      }
      case 3: {
        const Box fov = row(kFov, h, gap);
        label({content + 12, fov.y0 + 4, detail - 8, fov.y1 - 4},
              l10n::FormatFov(snap.language, snap.fov));
        slider({detail + 5, fov.y0 + 4, right - 12, fov.y0 + 28},
               std::clamp(snap.fov, 0, 100) / 100.0f, kHitFovSlider);
        const Box marks{detail, fov.y0 + 29, right - 8, fov.y1 - 4};
        label(marks.Column(0, 3, 4), t.fov_mark_default, 0.62f, muted);
        label(marks.Column(1, 3, 4), t.fov_mark_rec, 0.62f, muted, Align::kCenter);
        label(marks.Column(2, 3, 4), "+100%", 0.62f, muted, Align::kRight);
        break;
      }
      case 4: {
        const Box fps = row(kFps, 39.0f, 4.0f);
        label({content + 12, fps.y0 + 4, detail - 8, fps.y1 - 4}, t.frame_rate);
        const Box fps_choices{detail, fps.y0 + 5, right - 6, fps.y1 - 5};
        const char* rates[3] = {"60 FPS", "120 FPS", t.uncapped};
        constexpr int values[] = {60, 120, 0};
        for (int i = 0; i < 3; ++i) {
          chip(fps_choices.Column(i, 3, 6), rates[i], kHitFpsChoice, values[i],
               snap.fps == values[i]);
        }
        const auto note = next_row(24.0f, gap);
        label(note.Inset(12, 0), t.fps_note, 0.70f, muted);
        break;
      }
      case 5:
        toggle(kFpsCounter, t.fps_counter, snap.fps_counter, gap);
        break;
      case 6:
        toggle(kVsync, t.vsync, snap.vsync, gap);
        break;
      case 7:
        toggle(kTurbo, t.turbo_mode, snap.turbo, gap);
        break;
      case 8: {
        const std::string prompt_text =
            snap.prompt_choice == 0
                ? (std::string(t.prompt_auto) + " (" +
                   l10n::PromptStyleLabel(snap.language, snap.prompt_style) + ")")
                : l10n::PromptStyleLabel(snap.language, snap.prompt_choice);
        cycle_row(kButtonPrompts, t.button_prompts, prompt_text, kHitPromptChoice, gap);
        break;
      }
      case 9: {
        const Box config = row(kConfigureControls, h, gap);
        label({content + 12, config.y0 + 4, config.x1 - 42, config.y1 - 4},
              t.configure_controls);
        label({config.x1 - 34, config.y0 + 4, config.x1 - 8, config.y1 - 4}, ">", 0.85f, accent,
              Align::kCenter);
        break;
      }
      case 10:
        cycle_row(kLanguage, t.language_label, LanguageNativeName(snap.pending_language),
                  kHitLanguageChoice, gap, true);
        break;
      case 11: {
        const Box about = next_row(h, gap);
        const Box updates{right - 232, about.y0 + 6, right, about.y1 - 6};
        label({content + 12, about.y0 + 4, updates.x0 - 12, about.y0 + 28},
              l10n::FormatVersion(snap.language, snap.version_text), 0.74f, muted);
        label({content + 12, about.y0 + 29, updates.x0 - 12, about.y1 - 4}, t.made_by, 0.64f,
              muted);
        chip(updates, t.check_updates, kHitMainRow, kCheckUpdates, false, focused(kCheckUpdates));
        break;
      }
    }
  }

  if (can_scroll_up) {
    tri(center, 103.0f, center + 25.0f, 113.0f, center - 25.0f, 113.0f, line);
    tri(center, 104.6f, center + 17.25f, 111.5f, center - 17.25f, 111.5f, accent);
    register_hit({center - 40.0f, 101.0f, center + 40.0f, 115.0f}, kHitMainScrollUp, 0);
  }
  if (can_scroll_down) {
    tri(center - 29.0f, 603.0f, center + 29.0f, 603.0f, center, 617.0f, line);
    tri(center - 22.37f, 604.5f, center + 22.37f, 604.5f, center, 615.3f, accent);
    register_hit({center - 44.0f, 600.0f, center + 44.0f, 621.0f}, kHitMainScrollDown, 0);
  }

  fill({left, 624, right, 625}, line);
  label({left, 627, right, 650}, snap.toast, 0.76f, ink, Align::kCenter);
  label({left, 652, right, 676}, t.footer_hint, 0.72f, muted, Align::kCenter);

  if (modal) {
    fill({0, 0, width, 720}, Rgba(0, 0, 0, 146));
    const Box dialog{center - 248, 242, center + 248, 478};
    framed(dialog, accent, Rgba(30, 26, 32));
    const char* modal_title = snap.view == View::kConfirmLanguageRestart
                                  ? t.lang_restart_title
                                  : (snap.view == View::kConfirmRestart ? t.restart_title
                                                                         : t.quit_title);
    const char* modal_body = snap.view == View::kConfirmLanguageRestart
                                 ? t.lang_restart_body
                                 : t.unsaved_lost;
    label({dialog.x0 + 24, 264, dialog.x1 - 24, 310}, modal_title, 1.32f, ink, Align::kCenter);
    label({dialog.x0 + 24, 320, dialog.x1 - 24, 360}, modal_body, 0.86f, muted, Align::kCenter);
    const Box buttons{dialog.x0 + 32, 394, dialog.x1 - 32, 446};
    chip(buttons.Column(0, 2, 12), t.yes, kHitQuitYes, 0, false, snap.confirm_quit_yes);
    chip(buttons.Column(1, 2, 12), t.no, kHitQuitNo, 0, false, !snap.confirm_quit_yes);
  }
}

}  // namespace dwg2::recomp_menu
