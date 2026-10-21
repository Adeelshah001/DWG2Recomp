# To-Do

Future features planned for this recomp. These are goals to investigate as the
project progresses, not committed timelines. Add new ideas using the template below.

When an item is implemented successfully, move its entire entry to **Completed**
at the very bottom and strike through its title, status, description and notes.
Keep any remaining validation caveats in the entry.

---

## 2. Fast Forward Without Changing Music
- **Status:** Planned
- **Description:** Speed up game play while music continues at its normal speed
  and pitch. Preserve reliable audio playback when entering or leaving fast forward.

## 3. Faster Saves, Loading and Automatic Startup
- **Status:** Planned
- **Description:** Reduce save/load delays, add auto-save and streamline startup
  so returning players can continue without the repeated save/load selection
  screens. Load the existing save automatically when available.
- **Notes:** Preserve data integrity and existing saves. Handle first-time setup,
  missing or damaged saves and failed writes explicitly. Skipping prompts must
  not discard progress or pretend a save succeeded.

## 4. Fix Background Music Stopping
- **Status:** Investigating; no confirmed gameplay fix
- **Description:** Background music stops after a few minutes of mission play,
  while other audio continues, and resumes when the soundtrack changes.
- **Notes:** A previous isolated XMA synchronization candidate passed a host
  regression, but it is not present in the current runtime and has not been
  proven to fix this symptom. Verify stream refill, loop state and ownership of
  producer/consumer offsets before promoting a fix. Include several track loops
  and pause/resume in eventual gameplay validation.

## 5. Intermediate Resolution Options
- **Status:** Planned
- **Description:** Add resolution choices between the current 1x, 2x and 3x
  options, with matching internal rendering and output sizes. Keep switching
  available while the game is running.
- **Notes:** First requested option is true 1.5x internal rendering and 1920x1080
  output. Renderer investigation comes first: the current scale, tiled resolve
  storage and live migration paths use integer scales. An output-only 1080p
  mode does not complete this item. Validate fractional target dimensions,
  clipping, texture resolves and scale changes without restarting.

## 6. Automatic GitHub Release Updates
- **Status:** Planned
- **Description:** Check GitHub Releases at application startup and tell the
  player when a newer compatible build is available. Ask before downloading or
  installing the launcher update, following the update flow requested from the
  Blue Dragon and Lost Odyssey recomp projects.
- **Notes:** Preserve imported game files, saves, controls and display settings;
  verify downloaded packages and retain rollback support. Allow skipping an
  update and keep offline startup usable. Handle access to private releases
  without embedding credentials in the launcher.

## 7. PlayStation 3 ISO Import Alongside Xbox 360
- **Status:** Future research; not implemented
- **Description:** Let the user import a legally obtained PS3 or Xbox 360 copy
  of Dynasty Warriors: Gundam 2 through the same launcher and produce a playable
  PC installation with the same user-facing executable and features.
- **Notes:** This requires a separate PS3 recompilation backend and validation
  of the PS3 executable, PPU/SPU code, graphics, audio and platform services.
  An ISO conversion or renaming the Xbox 360 input cannot provide this support.
  Investigate shared launcher/runtime interfaces and edition compatibility;
  identical PC binary output and cross-version save compatibility are not assumed.
  Current setup supports only the qualified original USA Xbox 360 version.

## 8. Correct HUD Seams at Higher Internal Resolutions
- **Status:** Investigating; no confirmed fix
- **Description:** Remove faint rectangular edges and seams around HUD bars,
  portraits and counters at higher internal scales while preserving 3D detail.
- **Notes:** Compare identical HUD draws at 1x/2x/3x and future 1.5x. Identify
  whether sampling, atlas borders, pixel-center alignment or clipping causes
  the defect. The renderer already has half-pixel correction; do not apply a
  global offset without draw-level evidence. Asset replacement is not yet
  established as necessary.

## 9. PlayStation Button Prompts
- **Status:** Planned; reference approach investigated
- **Description:** Add Auto, Xbox and PlayStation prompt choices under Controls,
  independently of keyboard/controller remapping and confirm/cancel bindings.
- **Notes:** Identify this game's prompt textures and icon regions locally.
  Replace only verified button regions with original host-drawn glyphs while
  preserving UVs, alpha and atlas padding; retain original prompts on unknown
  textures. Study Lost Odyssey Recomp's atlas identification approach without
  reusing its game-specific atlas coordinates or distributing game assets.

---

## Template (copy for new items)

```markdown
## N. Feature Title
- **Status:** Planned
- **Description:** What it does and why.
```

---

## Completed

### ~~1. Controller Remapping and Controls Menu~~

- ~~**Status:** Implemented in v0.2 Alpha; user validation pending~~
- ~~**Description:** Add a Controls tab to the menu bar with controller remapping, saved mappings and a way to restore defaults. Keep keyboard and controller usable together, including the PS5 touch-pad full screen shortcut.~~
- ~~**Notes:** Controls > Configure provides keyboard, PS5 and Xbox profiles, click-to-bind buttons/sticks/triggers, deadzones, defaults and saved mappings.~~
