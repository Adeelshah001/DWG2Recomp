# Dynasty Warriors: Gundam 2 Recomp

Windows recompilation project for the original USA Xbox 360 release. This is a **development preview**, not a validated stable release.

This repository contains the launcher, original host integrations, build recipes and patches against the pinned open-source ReXGlue SDK. It contains **no game files, generated game translations, assets, saves, Xbox SDK or keys**. Supply your own legally obtained game locally.

Build `START_DWG2.exe` with `scripts/build-launcher.ps1`. On the first run, select your ISO or an extracted `default.xex` beside its original `data` and `Res` folders. Import is read-only with respect to the source, validates the supported executable and installs to `game-data`. Failed or cancelled imports do not replace an existing installation. The local build then creates `bin/dwg2.exe`. Later runs reuse it without asking for the game again. See [BUILDING.md](BUILDING.md) for the development toolchain required for that local build.

The launcher skips startup movies, enables keyboard and controller together, and uses the game's original 60 Hz timing without a frame-rate question. Native Windows menus provide:

- **File:** Pause / Resume (the game's Start button during gameplay), Quit Game.
- **View:** Fullscreen; 1x (1280x720), 2x (2560x1440), 3x (3840x2160) window size. Window resizing is immediate; internal GPU rendering scale applies on the next launch.
- **Help:** Controls and About.

Fullscreen hides the menu bar. Click the PS5 touchpad or press Alt+Enter to toggle fullscreen. The touchpad is consumed as a host shortcut and does not also send Xbox Guide.

The renderer's shader cache is retained between launches. Higher internal resolutions increase GPU cost; no whole-game stable-60 claim has been established. Save using the original game menu before quitting. The inherited runtime shutdown terminates the title and exits the process; this is not an automatic-save feature.

Updates replace only the launcher/runtime allowlist. Run the included `apply-update.ps1 -GameFolder <installed folder> -Package <extracted update folder>` after closing the game. Imported data, saves and caches remain local. Never upload your installation or private build outputs.

The full decompilation investigation is paused and is outside this repository. The private known-good executable remains the baseline while this preview receives gameplay validation.
