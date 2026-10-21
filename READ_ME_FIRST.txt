DWG2Recomp v02 LOCAL PREVIEW - not a new GitHub release

Close the game and launcher, extract the update, then run:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\apply-update.ps1 -GameFolder "C:\Projects\Dynasty Warriors - Gundam 2 Recomp\PrivateWorkspace\runtime\local build" -Package .
The update preserves your imported game, saves, cache and display settings.

Run START_DWG2.exe in local build. No game reimport or rebuild is needed.

Controls > Configure Controller / Keyboard opens the mapping editor.
Choose Keyboard, PS5 / DualSense or Xbox controller. Select a game control and
click Bind selected, or double-click its row, then press the desired key,
button, trigger or stick direction. Escape cancels binding. Apply saves and
uses mappings immediately. Save and close applies and closes. Cancel discards
unapplied edits. Restore defaults affects the selected profile until Apply.
The deadzone slider applies to sticks and triggers. Saved profiles are in
controls.ini. Keyboard and controller remain enabled together. Release held
buttons when closing the editor. Refresh devices after connecting a controller.
PS5 touchpad defaults to fullscreen and is remappable; Alt+Enter always remains
available as a host shortcut.

View > Resolution: 1x=1280x720, 2x=2560x1440, 3x=3840x2160.
Both internal rendering and window resolution change without restarting the game.
The GPU applies the change between frames before the window is resized and the
setting is saved. Fullscreen uses the monitor's desktop size for output.
A change can briefly pause while GPU resources and shader pipelines rebuild.
Transient render-target history is rebuilt; real-game validation is pending.
Unsafe migration layouts or allocation failures retain the prior scale.

File > Pause / Resume sends Start independently of the saved key bindings.
Quit Game, fullscreen menu hiding, no intro and no FPS question are retained.
Save with the game's own menu before quitting. Autosave is not implemented.

Gameplay and FPS testing are paused at your request. No stable-60 or physical
controller validation claim is made. Test this local preview before approving
publication as v02 alpha. GitHub's v0.1 Alpha release is unchanged.

Fresh source installations still need BUILDING.md's toolchain and your own game.
No game data, precompiled game executable or generated translation is in updates.
