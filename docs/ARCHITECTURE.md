# Architecture overview

This document explains how the DWG2Recomp system fits together — from the original Xbox 360 game image to the Windows executable the player runs.

## High-level data flow

```
┌─────────────────────┐
│  USA default.xex    │  (player's legal ISO, never in repo)
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐     ┌─────────────────────┐
│  ReXGlue generator  │────▶│  Translation units  │  (C++ .cpp files, one per guest function)
│  (dwg2-generator)   │     │  dwg2_recomp.N.cpp  │
└──────────┬──────────┘     └──────────┬──────────┘
           │                           │
           │              ┌────────────┴────────────┐
           │              ▼                         ▼
           │     ┌─────────────────┐         ┌─────────────────┐
           │     │  Host layer     │         │  SDK engine/GPU │
           │     │  (main.cpp,     │         │  (rexruntime,   │
           │     │   hooks, incs)  │         │   rexgpu-xenos) │
           │     └────────┬────────┘         └────────┬────────┘
           │              │                         │
           ▼              ▼                         ▼
    ┌─────────────────────────────────────────────────────────┐
    │                    clang / lld-link                     │
    │              (llvm-mingw 20261006, LLVM 23.1.3)         │
    └────────────────────────────┬────────────────────────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │    dwg2.exe     │  (native x64 runtime)
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  C# Launcher    │  (DWG2R.exe)
                        │  - ISO import   │
                        │  - Preparation  │
                        │  - Updates      │
                        │  - Settings     │
                        └─────────────────┘
```

## Key components

### 1. Generator (dwg2-generator)
Built from `src/generator.cpp` against the pinned ReXGlue SDK. Reads `config/dwg2.toml` (function list for the original USA `default.xex`, no game content) and emits one C++ translation unit per guest function. Output: `generated/dwg2_recomp.N.cpp` plus `dwg2_pch.h`.

### 2. Host layer (`src/main.cpp` + includes)
The host-side C++ that owns the window, app lifetime and the guest hooks. It contains:
- Window/app bootstrap (via `rex_app.cpp`, `windowed_app_main_sdl.cpp` from SDK)
- Save write integrity guard (`save_write_guard.inc`)
- Input/renderer hooks (`input_config.h`, `d3d12_*`, `fov_*`, etc.)
- Guest menu bridges (`preparation/GuestMenuAdapter.cs` ↔ `main.cpp`)

No guest code (`sub_*`) lives here — every guest symbol comes from generated TUs.

### 3. SDK patches (`src/*.patch`, `src/portable-sdk/*.patch`)
Exact-context unified diffs against the pinned ReXGlue SDK at revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. Applied to a **private copy** by `scripts/prepare-portable-sdk.py`; never edit an SDK checkout by hand.

### 4. Portable preparation capsule
Assembled by `scripts/assemble-portable.py` from the built SDK. Contains:
- clang, lld, llvm-ar (minimal set)
- Headers (`include/`, `headers/`) — **path list is part of the cache key**
- Static libraries (`libdwg2-host.a`, `librexruntime.dll.a`, `libfmt.a`, `libspdlog.a`, …)
- Codegen tool (`dwg2-prepare.exe`)
- `build.json` with per-file SHA-256 list, compile/link commands, `buildId`

The capsule is machine-independent: `assemble-portable.py` rejects absolute paths in flags.

### 5. C# Launcher (`DWG2R.exe`)
Pure .NET Framework 4.x. Responsibilities:
- **Setup / ISO import**: detects supported USA `default.xex`, runs the portable preparation
- **First-time preparation**: invokes the capsule's `dwg2-prepare.exe` → generates TUs → compiles → links `dwg2.exe`
- **Reuse**: compares `bin/portable-build.txt` (buildId + file hashes) to skip unchanged work (`generationReused`)
- **Menus**: File (import, quit), View (fullscreen, resolution 1x/2x/3x, FOV), Help (updates, about)
- **Controls**: keyboard, Xbox, PlayStation; remapping; button prompts
- **Updates**: polls GitHub `Adeelshah001/DWG2Recomp/releases`; verifies anonymous `sha256:` digest; downloads `DWG2Recomp.zip` or `DWG2Recomp-*-player.zip`; launches `maintenance/Apply-DownloadedUpdate.ps1`
- **Settings persistence**: `settings.ini`, `user-data/`, `bin/`, `controls.ini*`

### 6. Update application (`scripts/apply-portable-update.ps1`)
- Verifies installed `default.xex` hash matches supported USA image
- Refuses reparse-point paths
- Preserves `settings.ini`, `bin/`, `user-data/`
- Skips `settings.ini` when present
- Preserves `bin/portable-build.txt` if coherent stamp matches
- Uses `maintenance/UpdateProgress.exe` for visible progress

## Preparation cache key

The player's preparation cache key is a SHA-256 of:
1. Toolchain hashes (clang, lld, llvm-ar)
2. **`include/` + `headers/` directory path lists** (not file contents)
3. Compile flags from `config/portable-compile-flags.json`
4. Prefix maps (`-ffile-prefix-map=...`)
5. PCH dependency key

Changing any of these forces a full recompilation on the player's machine. **Adding a new path under `include/` or `headers/` is a cache-busting change.**

## Build paths

| Path | What it builds | Needs |
|------|----------------|-------|
| **Path 1** `build-launcher.ps1` | `DWG2R.exe` only | .NET Framework 4.x (`csc.exe`) |
| **Path 2** `build-local.py` | `dwg2.exe` + runtime DLLs | Full SDK, toolchain, ISO |
| **Path 3** `assemble-portable.py` | Portable preparation capsule | Built SDK (output of Path 2) |

See `BUILDING.md` for step-by-step commands.

## File ownership

| Area | Owned by |
|------|----------|
| Launcher, setup, menus, controls, updates | `launcher/` |
| Host hooks, save guard, in-game graphics/menu behaviour | `src/main.cpp` and `src/*.inc`, `src/*.h` |
| Engine, renderer, audio, input, SDK behaviour | `src/*.patch`, `src/portable-sdk/*.patch` |
| Build, patch, update tooling | `scripts/` |
| Documentation | `README.md`, `BUILDING.md`, `docs/` |

## No-game-content rule

The checkout must never contain:
- ISO, `default.xex`, extracted assets
- Generated title code (`*_recomp.*`, `*_funcs.*`, `*_init.*`, `*_register.*`)
- Compiled objects, caches, logs, PDBs
- Saves, `user-data/`, `bin/`, `settings.ini`
- Release ZIPs

`scripts/check_no_game_data.py` enforces this in CI.