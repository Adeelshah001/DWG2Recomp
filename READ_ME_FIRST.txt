DWG2Recomp v0.1 Alpha - Windows x64

FRESH SETUP: This alpha is a launcher/source kit, not a self-contained installer.
Read BUILDING.md and configure the local development toolchain first. Then run
START_DWG2.exe and select your own USA Xbox 360 ISO or extracted default.xex.
It imports the game once and builds bin/dwg2.exe locally. Later launches reuse
the installed game without asking again. No game executable or assets are shipped.

EXISTING INSTALL: Use the separate update ZIP. Close the game and launcher,
extract the update, then run:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\apply-update.ps1 -GameFolder "C:\path\to\installed-game" -Package .
Select the folder containing START_DWG2.exe. Game data, saves, cache and display
settings stay in place. This alpha update changes the launcher version/help text;
it retains the existing runtime and makes no performance changes.

Features retained: no intro, keyboard and controller together, no FPS prompt,
original 60 Hz timing, File / View / Help menu, Pause / Resume, Quit Game,
fullscreen with hidden menu, PS5 touchpad or Alt+Enter fullscreen toggle.
Window sizes: 1x=1280x720, 2x=2560x1440, 3x=3840x2160. Internal render scaling
applies next launch. Use 1x for the lowest GPU cost.

Save through the game's own menu before quitting. Autosave is not implemented.
Alpha limitations: stable 60 FPS is not established; extended mission/audio,
save/reload and physical controller validation remain pending. Gameplay and FPS
testing are paused at the user's request. Wishlist ideas are in WISHLIST.md.

Never upload your installation, ISO, game-data, generated game code or saves.
Release-posting instructions: RELEASE_GUIDE.md in the full launcher/source kit.
