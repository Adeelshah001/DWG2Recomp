> **AI usage disclosure:** This recompilation project was developed entirely using ChatGPT Codex (OpenAI). The original game and its assets remain the work of their respective creators.

<p align="center">
  <img src="assets/logo.png" alt="Dynasty Warriors: Gundam 2" width="400">
</p>
<p align="center">A Windows PC recompilation of Dynasty Warriors: Gundam 2.</p>
<p align="center"><a href="https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.4.1-alpha"><strong>Download v04.1 Alpha</strong></a></p>

## Features

- Keyboard, Xbox and PlayStation controller support with remappable controls.
- Xbox and PlayStation button prompts and controller diagrams.
- Live 1x, 2x and 3x internal rendering and output resolution.
- Live field-of-view slider, from Default to +50%. (Experimental)
- Fullscreen, pause/resume and restart from the menu bar.
- One-time ISO import, prepared-game reuse and skipped startup movies.
- Startup save loading and automatic slot selection on supported save screens.
- Automatic update checks and **Help > Check for Updates**.
- Native-style title-screen Quit Game confirmation with B / Circle.
- In-game Graphics/Video options and one editable **settings.ini**.
- **DWG2R.exe** launcher with desktop shortcut creation and automatic shortcut/icon refresh on updates.

**Alpha status:** The reported resolution-switching memory-exhaustion freeze is addressed in v04. Music interruptions, longer-session stability, expanded-FOV visibility and HUD issues remain under investigation. Save behavior still needs wider gameplay testing. Stable 60 FPS is not guaranteed; true 1080p/1.5x rendering is not included at the moment.

## Feature previews

### Quit Game confirmation

At PRESS START, B / Circle opens the native-style confirmation popup.

![Quit Game confirmation](assets/features/quit-game.png)


### In-game Graphics/Video options

Display and camera preferences are available in-game and in settings.ini. Menu appearance is still being refined.

![In-game Graphics / Video settings](assets/features/graphics-options.png)


### Field of view

The existing FOV slider supports Default through +50%. Expanded-FOV visibility remains experimental.

![FOV slider showcase](assets/features/fov.gif)


## Requirements

- Windows 10 or 11, 64-bit.
- A 64-bit CPU with SSE4.1 support and a DirectX 12-capable GPU.
- Your own legally obtained, original **USA Xbox 360 ISO** of Dynasty Warriors: Gundam 2. Other regions and modified images are not supported by this build.
- A writable local folder with space for imported game data and temporary setup files.

No game files are included. No separate developer tools or SDK installation is required. Minimum performance specifications are still being tested.

## Quick start

1. Download **DWG2Recomp-v04.1-alpha-player.zip** from [v04.1 Alpha](https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.4.1-alpha).
2. Extract the entire ZIP into a writable folder. Keep its contents together.
3. Run **DWG2R.exe** and select your supported ISO.
4. Allow the first-time import and preparation to finish. Later launches reuse the prepared game.
5. Open **Controls** to configure inputs, **View** for resolution/fullscreen, or **FOV** to adjust the field of view.

Save through the game's menus before quitting or restarting. Saves and settings stay in your local installation. Existing v03 installations can check for newer releases through **Help > Check for Updates**; an accepted update waits until the game and launcher close before applying.

## License and credits

Built with [ReXGlue](https://github.com/rexglue/rexglue-sdk), based on [Xenia](https://github.com/xenia-project/xenia). Bundled third-party components retain their respective licenses; see [license notices](THIRD_PARTY_LICENSE.txt).

[License/source archive](https://github.com/Adeelshah001/DWG2Recomp/releases/download/v0.3-alpha/DWG2Recomp-v02-v03-license-source.zip) — corresponding SDK/dependency source, build definitions and host-only relink materials for v02/v03. v04 host additions are under `licenses/v04-host-source.zip` in the player/update package. **Not required to play.**

This is an unofficial fan project. The original game, logo and trademarks belong to their respective rights holders. No rights to the original game content are granted by this project.
