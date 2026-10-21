# Dynasty Warriors: Gundam 2 Recomp

Windows recompilation project for the original USA Xbox 360 release. This checkout contains **v0.2 Alpha**, published at the user's request. Physical-controller and in-game resolution validation remain pending. Stable 60 FPS has not been established.

This repository contains the launcher, original host integrations, build recipes and patches against the pinned open-source ReXGlue SDK. It contains **no game files, generated game translations, assets, saves, Xbox SDK or keys**. Supply your own legally obtained game locally.

Build `START_DWG2.exe` with `scripts/build-launcher.ps1`. On the first run, select your ISO or an extracted `default.xex` beside its original `data` and `Res` folders. Import is read-only with respect to the source, validates the supported executable and installs to `game-data`. Failed or cancelled imports do not replace an existing installation. The local build then creates `bin/dwg2.exe`. Later runs reuse it without asking for the game again. See [BUILDING.md](BUILDING.md) for the development toolchain required for that local build.

The launcher skips startup movies, enables keyboard and controller together, and uses the game's original 60 Hz timing without a frame-rate question. Native Windows menus provide:

- **File:** Pause / Resume (the game's Start button during gameplay), Quit Game.
- **View:** Fullscreen; Resolution: 1x (1280x720), 2x (2560x1440), 3x (3840x2160). Internal rendering changes at a GPU frame boundary; the window follows after the renderer confirms success. No game restart. Fullscreen output uses the monitor's desktop size.
- **Controls:** Configure keyboard, PS5/DualSense and Xbox profiles. Double-click a control and press a key, controller button, trigger or stick direction; Apply or Save and close. Defaults and per-profile deadzones are available. Mappings are saved to `controls.ini` and take effect immediately. Release held buttons after leaving the editor.
- **Help:** Controls and About.

Fullscreen hides the menu bar. PS5 touchpad click toggles fullscreen by default and can be remapped. Alt+Enter remains a reserved host shortcut. Keyboard and controller stay enabled together. The editor blocks game input while binding controls. Refresh devices after connecting a controller while the editor is open.

The renderer's shader cache is retained between launches. Higher internal resolutions increase GPU cost; no whole-game stable-60 claim has been established. Save using the original game menu before quitting. The inherited runtime shutdown terminates the title and exits the process; this is not an automatic-save feature.

Updates replace only the launcher/runtime allowlist. Run the included `apply-update.ps1 -GameFolder <installed folder> -Package <extracted update folder>` after closing the game. Imported data, saves and caches remain local. Never upload your installation or private build outputs.

The full decompilation investigation is paused and is outside this repository. The private known-good executable remains the baseline while this alpha receives gameplay validation.

Resolution changes rebuild scale-dependent render targets, textures and pipelines and may cause a brief pause. Shared GPU memory and the guest session remain in place; scaled resolved textures are migrated through native-resolution readback. Transient render-target history is rebuilt. Unsupported migration layouts or allocation failures reject the change. These paths require the user's real-game validation; host-only checks do not establish whole-game graphics correctness.

The alpha launcher/source ZIP needs the local toolchain in [BUILDING.md](BUILDING.md).
The smaller update ZIP updates an existing compatible installation without a new
game import. The v02 alpha update replaces the launcher, runtime and GPU plugin;
gameplay and FPS testing remain paused. See [WISHLIST.md](WISHLIST.md) for future features
and [RELEASE_GUIDE.md](RELEASE_GUIDE.md) for step-by-step release instructions.
