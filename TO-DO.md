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
- **Status:** Startup autoload implemented locally; in-game validation pending; save speed/autosave remain planned
- **Description:** Reduce save/load delays, add auto-save and streamline startup
  so returning players can continue without the repeated save/load selection
  screens. Load the existing save automatically when available.
- **Notes:** Preserve data integrity and existing saves. Handle first-time setup,
  missing or damaged saves and failed writes explicitly. Skipping prompts must
  not discard progress or pretend a save succeeded.
  Startup now uses the original load operation for occupied slot 01, only in the
  startup load controller. Missing or structurally invalid data keeps the normal
  UI. Original slot application and readiness/error paths remain; cosmetic load
  delays are shortened. Manual saving/loading and autosaving are not changed.
  Original-state-machine comparison and 14 scope/guard cases pass with explicit
  UI/storage/deserializer doubles; full startup validation is still required.

## 4. Fix Background Music Stopping
- **Status:** Local synchronization fix implemented; mission validation pending
- **Description:** Background music stops after a few minutes of mission play,
  while other audio continues, and resumes when the soundtrack changes.
- **Notes:** The local runtime now serializes decoder and guest XMA context
  access and preserves the guest-owned output consumer offset. Current-ABI
  regression passes 200,000 concurrent record updates; restoring the stale
  write fails the negative control. This corrects a verified race, but the
  reported audible symptom still needs mission validation across several
  track loops and pause/resume before this item is completed.

## 5. Intermediate Resolution Options
- **Status:** Planned
- **Description:** Add resolution choices between the current 1x, 2x and 3x
  options, with matching internal rendering and output sizes. Keep switching
  available while the game is running.
- **Notes:** First requested option is true 1.5x internal rendering and 1920x1080
  output. Renderer investigation comes first: the current scale, tiled resolve
  storage and live migration paths use integer scales. An output-only 1080p
  mode does not complete this item. Validate fractional target dimensions,
  clipping, texture resolves and scale changes without restarting. Exact
  rational coordinate/resource arithmetic is implemented, with 32,768
  independent coverage checks passing; current integer scissor mapping uses
  it. Experimental 3/2 GPU tile packing and tiled readback now pass for
  8/16/32/64-bit pixels, including odd dimensions and partial tiles, while
  retaining every high-resolution sample. Connecting this format to active
  render targets, resolves, texture loads and migration remains unfinished, so no
  1.5x menu option is exposed yet. Fixed stale viewport caching on live changes.
  Rational scene color/depth allocation and viewport/scissor folding now draw
  true 1920x1080 fragments in a headless GPU fixture. The rendered scene passes
  through retained 3/2 tiles and native tiled readback with every sample checked.
  Existing integer scales and live migration pass regression. Production
  render-target ownership, texture-load formats, MSAA and live alias migration
  still need integration; the player menu therefore remains 1x/2x/3x.

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
- **Status:** Local atlas-specific sampling correction implemented; visual validation pending
- **Description:** Remove faint rectangular edges and seams around HUD bars,
  portraits and counters at higher internal scales while preserving 3D detail.
- **Notes:** Compare identical HUD draws at 1x/2x/3x and future 1.5x. Identify
  whether sampling, atlas borders, pixel-center alignment or clipping causes
  the defect. The renderer already has half-pixel correction; do not apply a
  global offset without draw-level evidence. Asset replacement is not yet
  established as necessary. View > HUD seam correction uses point/base-level
  sampling only for fingerprinted HUD atlases when internally scaled; original
  world filtering and native 1x sampling are preserved. The toggle permits
  comparison with original filtering. User confirmation of the reported seams
  is required before this item moves to Completed.

---

## 10. Investigate Background/Idle Crash
- **Status:** Investigating; original-fault diagnostics added locally
- **Description:** Identify and fix the reported background crash with launcher
  error -1073741795 (0xC000001D).
- **Notes:** The supplied crash record confirms an illegal-instruction exception
  re-raised through the game entry wrapper. Its original failing instruction is
  absent from that dump. The local diagnostic build logs the original module
  and offset before rethrow. The root cause and a fix are not confirmed.

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

### ~~9. PlayStation Button Prompts~~

- ~~**Status:** Implemented in the local build; in-game and physical-controller validation pending~~
- ~~**Description:** Controls > Button Prompts provides Auto, Xbox and PlayStation choices independently of remapping and confirm/cancel bindings. Auto follows actual keyboard/controller activity.~~
- ~~**Notes:** Original host-drawn glyphs replace identified combat/menu face-button, shoulder, trigger and stick/menu icon regions; unknown textures retain their original pixels. Nine headless GPU upload/readback style transitions pass and preserve guest texture bytes. Settings persist in prompts.ini. Localized game text is retained. The options diagram now follows last actual Xbox/PlayStation controller activity independently of keyboard activity and forced prompt choice; Xbox restores the exact original compressed image. PlayStation uses CC0 Kenney PS5 artwork and monochrome vector-derived glyphs. Forty GPU diagram cases pass across tiled/linear storage and four byte orders. No game textures are packaged.~~


### ~~11. Default and Extended Field of View~~

- ~~**Status:** Implemented locally; in-game visual validation pending~~
- ~~**Description:** View > Field of View offers Default and Extended (+10%), with live persistent changes.~~
- ~~**Notes:** Adjusts the perspective angle through the original projection function, preserving aspect, depth and orthographic HUD rendering. Thirty original projection cases pass; Default is bit-identical to original output.~~

### ~~12. Restart from the File Menu~~

- ~~**Status:** Implemented locally; user validation pending~~
- ~~**Description:** File > Restart closes the current process and starts a fresh game through the same launcher.~~
- ~~**Notes:** A session-specific restart event preserves the imported game and settings. Clean restart, normal quit and crash-after-request tests pass; crashes never trigger automatic relaunch. Save progress before restarting.~~
