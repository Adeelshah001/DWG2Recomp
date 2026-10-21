## ISO-only preparation (v0.2.1)

- Extract the complete ZIP into a fresh writable folder and select your supported USA ISO once. No separately installed development tools should be needed.
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
