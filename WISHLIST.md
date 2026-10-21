# Wishlist

Future features planned for this recomp. These are goals to investigate as the
project progresses, not committed timelines. Add new ideas using the template below.

---

## 1. Controller Remapping and Controls Menu
- **Status:** Implemented in v0.2 Alpha; user validation pending
- **Description:** Add a Controls tab to the menu bar with controller remapping,
  saved mappings and a way to restore defaults. Keep keyboard and controller
  usable together, including the PS5 touchpad fullscreen shortcut.
- **Notes:** Controls > Configure provides keyboard, PS5 and Xbox profiles,
  click-to-bind buttons/sticks/triggers, deadzones, defaults and saved mappings.

## 2. Fast Forward Without Changing Music
- **Status:** Planned
- **Description:** Speed up gameplay while music continues at its normal speed
  and pitch. Preserve reliable audio playback when entering or leaving fast forward.

## 3. Faster Saves, Loading and Automatic Startup
- **Status:** Planned
- **Description:** Reduce save/load delays, add autosave and streamline startup
  so returning players can continue without the repeated save/load selection
  screens. Load the existing save automatically when available.
- **Notes:** Preserve data integrity and existing saves. Handle first-time setup,
  missing or damaged saves and failed writes explicitly. Skipping prompts must
  not discard progress or pretend a save succeeded.

## 4. Intermediate Resolution Options
- **Status:** Planned
- **Description:** Add resolution choices between the current 1x, 2x and 3x
  options, with matching internal rendering and output sizes. Keep switching
  available while the game is running.

## 5. Automatic GitHub Release Updates
- **Status:** Planned
- **Description:** Check GitHub Releases at application startup and tell the
  player when a newer compatible build is available. Ask before downloading or
  installing the launcher update, following the update flow requested from the
  Blue Dragon and Lost Odyssey recomp projects.
- **Notes:** Preserve imported game files, saves, controls and display settings;
  verify downloaded packages and retain rollback support. Allow skipping an
  update and keep offline startup usable. Handle access to private releases
  without embedding credentials in the launcher.

---

## Template (copy for new items)

```markdown
## N. Feature Title
- **Status:** Planned
- **Description:** What it does and why.
```
