> [!NOTE]
> **AI usage disclosure:** This recompilation project was developed entirely using AI.

<p align="center">
  <img src="assets/logo.png" alt="Dynasty Warriors: Gundam 2" width="400">
</p>
<p align="center">A Windows PC recompilation of Dynasty Warriors: Gundam 2.</p>
<p align="center"><a href="https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.7-alpha"><strong>Download v07 Alpha</strong></a></p>

## Features

- Fixed a failed or short save write being reported to the game as a successful save.

- Keyboard, Xbox and PlayStation controller support with remappable controls.
- Xbox and PlayStation button prompts and controller diagrams.
- Live 1x(720p), 2x(1440p) and 3x(2160p) internal rendering and output resolution.
- Live field-of-view slider, from Default to +50%. (Experimental)
- Remembered fullscreen settings, L2/LT and F11 full-screen shortcuts, pause/resume and restart.
- One-time ISO import, prepared-game reuse and skipped startup movies.
- Incremental updates reuse unchanged compiled game parts.
- Startup save loading and automatic slot selection on supported save screens.
- Automatic update checks and **Help > Check for Updates**.
- Native-style title-screen Quit Game confirmation with B / Circle.
- In-game Graphics/Video options and one editable **settings.ini**.
- **DWG2R.exe** setup/launcher with a **DWG2R** desktop shortcut; existing shortcuts are retained on updates.
- Optional ISO detection from the **game** folder, plus frontmost setup and update progress.

**Alpha status:** Music interruptions, longer-session stability, expanded-FOV visibility and HUD issues remain under investigation. Save behavior still needs wider gameplay testing. Stable 60 FPS is not guaranteed; true 1080p/1.5x rendering is not included at the moment.

An already-affected interim checkpoint or save is not repaired by v07. If Restart fails or shows the wrong character, start the affected mission again from its original Official Mode or Mission Mode selection. Keep a save backup; the broader character/SP issue remains under investigation.

## Feature previews

<table>
  <tr>
    <th width="50%">Quit Game from Title Screen</th>
    <th width="50%">Graphics/Video Settings</th>
  </tr>
  <tr>
    <td width="50%"><img src="assets/features/quit-game.png" alt="Quit Game confirmation" width="100%"></td>
    <td width="50%"><img src="assets/features/graphics-options.png" alt="In-game graphics settings" width="100%"></td>
  </tr>
  <tr>
    <th colspan="2">Field of View (Default to +50%)</th>
  </tr>
  <tr>
    <td colspan="2"><img src="assets/features/fov.gif" alt="Field-of-view slider showcase" width="100%"></td>
  </tr>
</table>

## Requirements

- Windows 10 or 11, 64-bit.
- A 64-bit CPU with SSE4.1 support and a DirectX 12-capable GPU.
- Your own legally obtained, original **USA Xbox 360 ISO** of Dynasty Warriors: Gundam 2. Other regions and modified images are not supported by this build.
- A writable local folder with space for imported game data and temporary setup files.

No game files are included. No separate developer tools or SDK installation is required. Minimum performance specifications are still being tested.

## Quick start

1. Download **DWG2Recomp.zip** from [v07 Alpha](https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.7-alpha).
2. Extract the entire ZIP into a writable folder. Keep its contents together.
3. Optionally put your supported USA Xbox 360 ISO in the **game** folder, then run **DWG2R.exe**. If no supported ISO is found there, setup asks you to choose it.
4. Allow the first-time import and preparation to finish. Later launches reuse the prepared game.
5. Open **Controls** to configure inputs, **View** for resolution/fullscreen, or **FOV** to adjust the field of view.

Save through the game's menus before quitting or restarting. Saves and settings stay in your local installation. On v05 and later, use **Help > Check for Updates**. Save your progress before accepting the close-and-install prompt. Compatible updates keep your prepared executable or reuse cached compiled parts. Compiler/shared-interface changes can still require fuller preparation.

## Building from source

The launcher, host layer, scripts and SDK patches are all here. **No game content is
included and none is ever needed in a checkout**: the title executable is generated on
the machine that owns a supported ISO.

- [BUILDING.md](BUILDING.md) - toolchain, the pinned ReXGlue SDK, and the three build
  paths (launcher only, local runtime, player package).
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - how recompilation, the preparation cache
  and the updater fit together.
- [AGENTS.md](AGENTS.md) - the same material organised for AI coding assistants.

Keep your own legally obtained USA ISO outside Git. Do not commit game data, generated
title code, compiled objects, caches, saves or logs.

## Contributing

Bug reports, fixes and feature work are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md)
first. The short version: keep game content out of the repository, keep existing player
saves and settings working, and state plainly what you tested on real hardware.

## License

This project's own source - the host layer, launcher, scripts and SDK patches - is
licensed under the **GNU General Public License v3.0**; see [LICENSE](LICENSE). That is
the same license as the other Xbox 360 recompilation projects this work builds on.

Built with [ReXGlue](https://github.com/rexglue/rexglue-sdk), based on
[Xenia](https://github.com/xenia-project/xenia). Bundled third-party components keep
their own licenses (BSD-3-Clause, MIT and others); see
[license notices](THIRD_PARTY_LICENSE.txt). Those notices are not optional: they apply to
any build you distribute.

This is an unofficial fan project. The original game, logo and trademarks belong to their
respective rights holders. No rights to the original game content are granted by this
project, and no game code, asset or executable is distributed here.
