# v0.2 Alpha test checklist

Use START_DWG2.exe in local build. Save using the game's own menu before testing.

- [ ] Open Controls > Configure Controller / Keyboard. On Keyboard, change one action, Apply, and confirm the new key works and the old binding no longer triggers it (unless retained as an alternate).
- [ ] Repeat on a connected PS5 and/or Xbox controller. Check a face button, trigger and stick direction; confirm keyboard still works alongside the controller.
- [ ] Check PS5 touchpad fullscreen, Alt+Enter, and the menu hiding/reappearing. Test a custom fullscreen binding if desired.
- [ ] Restart normally: mappings must persist. Test Restore defaults + Apply. Cancel should discard unapplied edits. Release held inputs after closing the editor and check for stuck input.
- [ ] In a running scene, choose View > Resolution: 1x -> 2x -> 3x -> 1x, without restarting. Check the menu selection, sharper internal 3D rendering, matching window size, readable UI, and no black screen or missing textures. A brief rebuild pause is possible. Skip a size if it is impractical on your display.
- [ ] Toggle fullscreen and repeat a resolution change. Fullscreen output follows the desktop resolution; internal detail should change. Restart and confirm the last successfully selected resolution is retained.

Record the device/USB or Bluetooth connection, scene, resolution transition and any failure. FPS benchmarking remains paused. Host tests do not prove whole-game or physical-device correctness.
