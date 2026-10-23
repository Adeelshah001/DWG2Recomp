# v03 Alpha checks

Gameplay/FPS benchmarking remains paused. When you choose to test:

- Open Help > Check for Updates: on v03 it should report the latest available build.
- When a later release exists, check startup and manual prompts. Cross/A accepts;
  Circle/B declines. Release any held button first. Mouse/keyboard also work.
- Decline and confirm the current session continues. Accept downloads in the
  background, then save and quit normally: installation happens after exit.
- After the first manual v02-to-v03 update, expect one local preparation. Later
  starts reuse the prepared game; imported data and settings should remain.
- Finish a mission with a valid existing slot 01: the dedicated save screen
  should choose it automatically and retain normal write completion/errors.
  Reopen the game and confirm progress was saved. Missing/damaged saves retain
  the original UI. This has bounded automated coverage, not mission validation.
- FOV is a separate menu-bar item with a live Default to +50% slider in 5% steps.
- File > Pause Game should freeze/resume the runtime. Menus themselves keep it running.
- Music interruptions remain open: record mission timer, last healthy audio,
  pause/focus changes and keep the matching logs.
- Resolution remains live integer 1x/2x/3x; true 1080p is unfinished and not included.

## Later local preview (when manual testing resumes)

- [ ] With an existing slot 01, start through START_DWG2.exe and confirm startup
      loads it without selecting a slot. Check missing-slot fallback separately
      using a separate installation; preserve your original saves.
- [ ] After saving normally, use File > Restart. Confirm a fresh game session
      starts and settings persist. File > Quit should leave the game closed.
- [ ] Compare View > Field of View > Default / Extended (+10%) in the same scene.
      Check live changes, normal UI and persistence after restarting.
- [ ] Use an Xbox/PS5 controller and open the game's controls screen. Confirm
      the diagram follows controller activity and keyboard use retains it.
- [ ] Check Controls > Button Prompts and View > HUD seam correction. Record
      remaining incorrect glyphs, seams or music interruptions.
- [ ] If a crash recurs, retain the newest logs. The diagnostic line beginning
      `DWG2 native fault` identifies the original module and offset.

These checks are optional until gameplay testing resumes. These are manual validation instructions, not a claim of completed gameplay testing.

## Smaller player package

- Extract only the player ZIP; preparation should contain three files and no expanded compiler tree.
- On first setup, tools expand temporarily and disappear after preparation. The compressed payload remains for offline repair.
- Repeat launch and compatible updates should reuse the prepared executable.

## ISO-only preparation (v02 Alpha)

- Extract the player ZIP into a fresh writable folder and select your supported USA ISO once. No separately installed development tools should be needed.
- Let import/preparation finish; restart the launcher and confirm it reuses the prepared executable.
- Existing installation: apply the update from a separate extracted folder; confirm saves, controller mappings and display settings remain. A one-time compatible rebuild is expected.
- Gameplay/FPS testing remains paused. The following controls/resolution checks are for when you resume manual testing.

# v0.2 Alpha test checklist

Use START_DWG2.exe in local build. Save using the game's own menu before testing.

- [ ] Open Controls > Configure Controller / Keyboard. On Keyboard, change one action, Apply, and confirm the new key works and the old binding no longer triggers it (unless retained as an alternate).
- [ ] Repeat on a connected PS5 and/or Xbox controller. Check a face button, trigger and stick direction; confirm keyboard still works alongside the controller.
- [ ] Check PS5 touchpad fullscreen, Alt+Enter, and the menu hiding/reappearing. Test a custom fullscreen binding if desired.
- [ ] Restart normally: mappings must persist. Test Restore defaults + Apply. Cancel should discard unapplied edits. Release held inputs after closing the editor and check for stuck input.
- [ ] In a running scene, choose View > Resolution: 1x -> 2x -> 3x -> 1x, without restarting. Check the menu selection, sharper internal 3D rendering, matching window size, readable UI, and no black screen or missing textures. A brief rebuild pause is possible. Skip a size if it is impractical on your display.
- [ ] Toggle fullscreen and repeat a resolution change. Fullscreen output follows the desktop resolution; internal detail should change. Restart and confirm the last successfully selected resolution is retained.

Record the device/USB or Bluetooth connection, scene, resolution transition and any failure. FPS benchmarking remains paused. Host tests do not prove whole-game or physical-device correctness.
