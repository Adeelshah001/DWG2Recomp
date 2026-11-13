# AGENTS.md — DWG2Recomp repository invariants

This file is the single source of truth for any AI agent working in this repository.

## Project scope
- This is a **recompilation plus features** project (launcher, host layer, renderer hooks, in-game menus, update system).
- Byte-exact source recovery is **not** part of this project. Do not send reconstructed guest routines or "matches the original instructions" patches.
- The public tree contains only host/launcher code and reviewed SDK patches. No game content, generated code, compiled objects, caches, saves or logs ever enter the checkout.

## Repository layout

```
config/
  compiler.json          pinned SDK revision, host compile/link flags
  dwg2.toml              generator function list (original USA image only)
  engine-link.json       SDK engine link command template
  gpu-link.json          GPU backend link command template
  portable-compile-flags.json  portable capsule compile flags (no absolute paths)

launcher/                C# launcher: setup, ISO import, controls, menus, updates
  Launcher.cs            main entry, menus, version strings, update checks
  UpdateService.cs       GitHub release polling, anonymous SHA-256 verification
  SetupProgress.cs       first-time setup window
  PortableBuild.cs       portable capsule loading, generationReused logic
  GameSession.cs         title process launch, quit handling
  PlayerSettings.cs      controls, graphics, save/remap persistence
  DesktopShortcut.cs     shortcut creation
  UpdateInstallProgress.cs  standalone update progress window
  CompatibilityLauncher.cs  tiny wrapper for old shortcuts
  SettingsDefaults.ini   embedded defaults resource

preparation/             host-side source the launcher compiles into the guest build
  GuestMenuAdapter.cs    C#→guest bridge for graphics/controls menus
  menu-adapter-rules.json  declarative menu layout

scripts/                 build, patch and update helpers
  build-launcher.ps1     launcher-only build (no SDK needed)
  prepare-portable-sdk.py  copy pinned SDK + apply patches to private folder
  build-host-library.py  compile main.cpp + rex_app + windowed_app_main_sdl → libdwg2-host.a
  build-local.py         full local runtime build (needs SDK, toolchain, ISO)
  assemble-portable.py   build portable preparation capsule from built SDK
  apply-portable-update.ps1  player-side update application (verifies xex, preserves saves)
  check_no_game_data.py  CI gate: rejects game content in the tree

src/                     host layer (main.cpp + hooks) and SDK patches
  main.cpp               host entry, guest hooks (save guard, input, renderer)
  *.inc                  inline guest-side helpers included by main.cpp
  *.h                    host/guest interface headers
  *.patch                exact-context diffs against pinned SDK (src/ and src/portable-sdk/)
  portable-sdk/          SDK headers patched sources include

docs/
  ARCHITECTURE.md        system overview (guest ISO → generator → TUs → clang → dwg2.exe)
```

## Invariants

### Cache key (preparation/build.json)
The player's preparation cache key is a hash of:
- toolchain hashes (clang, lld, llvm-ar)
- **`include/` + `headers/` directory PATH LISTS** (not contents)
- compile flags from `config/portable-compile-flags.json`
- prefix maps
- pch dependency key

**Adding a new path under `include/` or `headers/` invalidates every player's cache.** Adding content to an existing file is safe. Do not add paths without discussion.

### Updater constraints
- `UpdateService.Repository` is hard-coded to `Adeelshah001/DWG2Recomp`. Forks testing updates must change it.
- The updater selects `DWG2Recomp.zip` (full) or `DWG2Recomp-*-player.zip`; requires GitHub `digest` `sha256:`.
- Only github.com hosts are allowed.
- `apply-portable-update.ps1`:
  - Verifies installed `default.xex` hash (`0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b`)
  - Refuses reparse-point paths
  - Preserves `settings.ini`, `bin/`, `user-data/`
  - Skips `settings.ini` when present
  - Preserves `bin/portable-build.txt` if coherent stamp matches

### Save/setting safety
- `src/save_write_guard.inc` guards the host-side write path. A failed save must **never** be reported as success.
- Player saves live in `user-data/` and `bin/saves/` — never touch these in the repo.
- Settings live in `settings.ini` — preserve on update; never overwrite without migration.

### Build isolation
- All generated files, objects, caches, logs stay **outside the checkout**. The scripts enforce this.
- `scripts/prepare-portable-sdk.py` refuses to write inside the SDK checkout or this repo.
- `scripts/build-local.py` requires a `--work` directory outside the repo.
- `scripts/assemble-portable.py` rejects absolute paths in flags and computes `buildId` from files+compile+link.

## Verification commands

```powershell
# 1. No game content in the tree (runs in CI)
python scripts/check_no_game_data.py

# 2. Launcher builds cleanly
powershell -ExecutionPolicy Bypass -File scripts/build-launcher.ps1 -Output .\DWG2R.exe

# 3. Whitespace / line-ending hygiene
git diff --check

# 4. Personal-path scan (run before committing)
python -c "
import re,pathlib
pat=re.compile(r'C:\\\\\\\\Projects|C:/Projects|/mnt/c/|\\\\\\\\Users\\\\\\\\|/Users/|riser|AppData',re.I)
for p in pathlib.Path('.').rglob('*'):
    if p.is_file() and '.git' not in p.parts and p.suffix.lower() not in ('.png','.gif','.ico'):
        t=p.read_text(encoding='utf-8',errors='replace')
        for i,line in enumerate(t.splitlines(),1):
            if pat.search(line):
                print(f'{p.as_posix()}:{i}: {line.strip()[:160]}')
"
```

## Paths that must never appear in the checkout
- `*.iso`, `*.xex`, `*.xexp`, `*.elf`
- `generated/`, `objects/`, `cache/`, `logs/`, `bin/`, `user-data/`, `tools/`, `game-data*/`
- `*.pdb`, `*.obj`, `*.pch`, `*.exe`, `*.dll`, `*.lib`, `*.a`, `*.o`
- `*_recomp.*`, `*_funcs.*`, `*_init.*`, `*_register.*`
- `*.zip` (release artifacts are external)

## License
- This project's own source: **GPL-3.0** (see `LICENSE`).
- Third-party components keep their own licenses: see `THIRD_PARTY_LICENSE.txt`.
- Any distributed build must carry those third-party notices.

## AI assistant disclosure
If you use an AI assistant, disclose it in the pull request and personally verify the build and the in-game behaviour you claim. Generated code that has never been compiled or run will be sent back.

## Where changes belong

| Change | Location |
|--------|----------|
| Setup, ISO import, menus, controls, updates, shortcuts | `launcher/` |
| Host hooks, in-game menu/graphics behaviour, save handling | `src/` (`main.cpp` and the `*.inc`/`*.h` it includes) |
| Engine, renderer, audio, input or SDK behaviour | `src/*.patch`, `src/portable-sdk/*.patch` |
| Build, patch or update tooling | `scripts/` |
| Documentation | `README.md`, `BUILDING.md`, `docs/` |

Patches are exact-context unified diffs against the pinned SDK revision. Keep context lines intact.