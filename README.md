# Dynasty Warriors: Gundam 2 Recomp

Windows alpha launcher for the supported original USA Xbox360 edition. Import
your own legally obtained ISO once; later launches reuse the prepared game.
Game files and generated game code/executables are never distributed here.

Download the single **DWG2Recomp-v03-alpha-player.zip** from
[v03 Alpha](https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.3-alpha),
extract it, and run **START_DWG2.exe**. Required preparation tools are bundled
compressed, expand temporarily and are cleaned up after successful preparation.
No separately installed development toolchain is required.

Existing users can apply the player package with its `apply-update.ps1`; imported
game data, saves and settings are preserved. v03 is the first launcher with an
automatic update check; installing it over v02 is a manual update. Subsequent
public GitHub releases can be offered automatically.

The startup update check runs concurrently with game launch. **Help > Check for
Updates** checks manually. The update dialog supports keyboard/mouse and SDL
controller face buttons: **Cross / A** accepts, **Circle / B** declines. Accepting
downloads and verifies the package, then installs only after the game and launcher
close. Save your progress before quitting. Offline or failed checks do not stop
gameplay. Releases must be public; no credentials are bundled.

Features include keyboard/PS5/Xbox remapping, controller prompts/diagrams, fullscreen
and PS5 touchpad shortcut, live1x/2x/3x internal/output resolution, live FOV slider,
true runtime pause/resume, restart and no startup intro. Dedicated save screens
automatically select slot01 using the original save operation when the existing
container is valid; Options save/load and recovery/error paths remain available.
First-time or invalid save containers retain the normal UI.

Known limitations: sustained music playback, background crashes, HUD correction,
post-mission autosave and physical controller update navigation still need user
gameplay confirmation. Stable60FPS is not certified. True1.5x/1080p rendering and
FSR are not included in v03; the existing integer renderer is retained.

See [TEST_CHECKLIST.md](TEST_CHECKLIST.md), [BUILDING.md](BUILDING.md),
[RELEASE_GUIDE.md](RELEASE_GUIDE.md) and [THIRD_PARTY_LICENSE.txt](THIRD_PARTY_LICENSE.txt).
An optional developer source/relink asset is separate from the player download.
