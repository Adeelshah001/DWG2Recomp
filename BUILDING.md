# Building DWG2Recomp

Two very different jobs live in this repository.

| You want to... | Read |
| --- | --- |
| Play the game | The [README](README.md). Nothing here needs to be built. |
| Change the launcher, menus, controls, updater or docs | [Path 1](#path-1--build-the-launcher-no-sdk) |
| Change the host layer, renderer hooks or game behaviour | [Path 2](#path-2--build-a-local-runtime) |
| Assemble something you can hand to other players | [Path 3](#path-3--player-package) |

**No game content belongs in this repository.** A checkout must never contain an ISO,
`default.xex`, generated title code, compiled objects, caches, saves or logs. The title
executable is generated on the machine that owns a supported ISO.

## What you need

Common to every path:

- Windows 10 or 11, 64-bit.
- .NET Framework 4.x — `csc.exe` ships with Windows and builds the launcher.
- Python 3.11 or newer — for the helper scripts in `scripts/`.
- Git.

Only for Path 2 and Path 3:

- [llvm-mingw 20261006 UCRT x86_64](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)
  (LLVM 23.1.3). Archive SHA-256
  `317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a`.
  Do not mix this GNU C++ ABI with MSVC-built host libraries.
- CMake 3.25 or newer and Ninja.
- The pinned [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) at
  `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, with submodules. `config/compiler.json`
  records that revision; the scripts refuse a different checkout.
- Your own legally obtained, original **USA** Xbox 360 ISO. The supported executable is
  `default.xex`, SHA-256 `0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b`.
  Other regions, title updates and modified images are rejected on purpose.

## Repository layout

```
config/        generator config (dwg2.toml), host/engine/GPU link recipes,
               portable compile flags, pinned SDK revision
launcher/      C# launcher: setup, ISO import, settings, controls, updates
preparation/   host-side source the launcher compiles into the guest build
scripts/       build, patch and update helpers
src/           host layer (main.cpp + hooks) and the reviewed SDK patches
docs/          architecture notes
```

`src/*.patch` and `src/portable-sdk/*.patch` are exact-context diffs against the pinned
SDK. They are applied to a *copy*; never edit an SDK checkout by hand and expect a build
to match.

## Path 1 — build the launcher (no SDK)

Everything a player sees — setup, ISO import, the File/View/Help menus, controls,
resolution, update checks — is the C# launcher. It needs no SDK and no game file.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-launcher.ps1 -Output .\DWG2R.exe
```

`build-launcher.ps1` compiles the seven sources in `launcher/` with the Windows .NET
Framework compiler and embeds `launcher/SettingsDefaults.ini` as a resource. An optional
icon can be supplied with `-IconPath`, or through `launcherIcon` in `toolchain.local.json`.

To try it, put the new `DWG2R.exe` into a **copy** of an existing installation, next to
`maintenance/`, `preparation/`, `bin/` and `game-data/`. The launcher rewrites prepared
files, so never experiment in the installation you care about.

Two things to know before editing it:

- Version strings live in `launcher/Launcher.cs` (`Version = "0.7-alpha"`) and
  `launcher/UpdateService.cs`.
- `UpdateService.Repository` is hard-coded to `Adeelshah001/DWG2Recomp`. A fork that wants
  to test updates must point it at its own repository; otherwise it will offer the
  upstream release.

## Path 2 — build a local runtime

This produces a private `dwg2.exe` plus the runtime DLLs. It needs the SDK, the toolchain
and your own ISO. Keep every generated file outside the checkout; the scripts enforce
that.

### 1. Get the pinned SDK

```powershell
git clone https://github.com/rexglue/rexglue-sdk.git C:\dwg2\sdk
git -C C:\dwg2\sdk checkout c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C C:\dwg2\sdk submodule update --init --recursive
```

### 2. Apply this project's patches to a private copy

```powershell
python scripts/prepare-portable-sdk.py --sdk-source C:\dwg2\sdk --output C:\dwg2\sdk-patched
```

The script verifies the revision, copies the tree, applies `src/*.patch` and
`src/portable-sdk/*.patch`, copies the headers patched SDK sources include into
`include/`, and places this repository's `src/` at `dwg2-host/src` inside the copy.
It refuses to write inside either source tree.

### 3. Configure and build the SDK

```powershell
cmake -S C:\dwg2\sdk-patched -B C:\dwg2\sdk-build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=C:\dwg2\llvm-mingw\bin\clang.exe `
  -DCMAKE_CXX_COMPILER=C:\dwg2\llvm-mingw\bin\clang++.exe `
  -DCMAKE_SYSTEM_PROCESSOR=AMD64 `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF `
  -DREXGLUE_USE_VULKAN=OFF -DREXGLUE_USE_D3D12=ON

cmake --build C:\dwg2\sdk-build --target rexruntime rexgpu-xenos dwg2-generator --parallel 4
```

`dwg2-generator` is the codegen tool built from this repository's `src/generator.cpp`
against the patched SDK. `compile_commands.json` is required later: the local build
reuses the SDK's own compile lines for the patched engine sources.

### 4. Build the host layer

```powershell
python scripts/build-host-library.py `
  --toolchain C:\dwg2\llvm-mingw `
  --sdk-source C:\dwg2\sdk-patched `
  --sdk-build C:\dwg2\sdk-build `
  --output C:\dwg2\out\libdwg2-host.a
```

This compiles `src/main.cpp` and the two SDK files that own the window and app lifetime
(`src/ui/rex_app.cpp`, `src/ui/windowed_app_main_sdl.cpp`) and archives them. It contains
no title code: every `sub_*` symbol comes from generated translation units.

### 5. Build the runtime

Copy `toolchain.example.json` to `toolchain.local.json` (ignored by Git) and fill in your
paths:

```json
{
  "clang": "C:\\dwg2\\llvm-mingw\\bin\\clang++.exe",
  "sdk_source": "C:\\dwg2\\sdk-patched",
  "sdk_build": "C:\\dwg2\\sdk-build",
  "generator": "C:\\dwg2\\sdk-build\\dwg2-generator.exe",
  "work": "C:\\dwg2\\work"
}
```

Then:

```powershell
python scripts/build-local.py --profile toolchain.local.json `
  --game-data C:\dwg2\game --output C:\dwg2\out
```

`--game-data` must contain `default.xex`; its hash is checked against the supported USA
image. The script applies the SDK patches into an isolated overlay, rebuilds the patched
engine/GPU sources, regenerates the title code from your ISO, compiles it with the
portable flags and links `dwg2.exe`. Nothing is uploaded and nothing is written inside
the checkout.

`scripts/build-game.ps1` is a thin PowerShell wrapper around the same script.

### 6. Run and test

`dwg2.exe` is the same runtime the player package installs as `bin/dwg2.exe`; the launcher
is what normally starts it. For quick iteration, replace files in a **copy** of an
installation, but expect the launcher to re-prepare when `bin/portable-build.txt` no
longer matches. Gameplay behaviour must be confirmed in the game itself — a successful
build says nothing about a fix.

## Path 3 — player package

The player package is a portable preparation capsule plus the launcher. Assemble the
capsule from the built SDK:

```powershell
python scripts/assemble-portable.py `
  --sdk-source C:\dwg2\sdk-patched `
  --sdk-build C:\dwg2\sdk-build `
  --toolchain C:\dwg2\llvm-mingw `
  --host-library C:\dwg2\out\libdwg2-host.a `
  --flags config\portable-compile-flags.json `
  --output C:\dwg2\portable
```

The result holds the compiler, headers, static libraries, codegen tool and `build.json`
with a per-file hash list and a `buildId`. It rejects absolute paths in flags, so the
capsule stays machine independent.

Current release assembly — the update manifest, the player ZIP layout, the release notes
and the qualification fixture — is maintainer-side and not part of this tree. If you want
to publish your own build, start from the capsule above plus `DWG2R.exe`; a packaging
script that reproduces the shipped layout would be a welcome contribution.

The authoritative record of exactly what shipped in a release is
`licenses/host-source.zip` inside the player package.

## Checks before you open a pull request

```powershell
python scripts/check_no_game_data.py     # no game content or generated code
powershell -File scripts/build-launcher.ps1 -Output .\DWG2R.exe
git diff --check
```

`check_no_game_data.py` also runs in CI on every push and pull request.

## Things that break other people's installs

- **Do not add files under `include/` or `headers/` in the portable capsule.** The
  preparation cache key includes those path lists, so a new path forces every player to
  recompile the whole game. Adding content to an existing file is safe; adding a new path
  is not.
- **Do not change `preparation/build.json` flags casually.** A different compile line
  changes `buildId`, which invalidates every player's cached objects.
- **Do not touch the update allow-list or stamp checks** without a rollback path:
  `scripts/apply-portable-update.ps1` verifies the installed game hash, refuses linked
  paths, and preserves `settings.ini`, `bin/` and `user-data/`.
- **Never make a save failure look like success.** Persistence code is deliberately
  conservative; see `src/save_write_guard.inc`.

## License

GPL-3.0 for this project's own source (see [LICENSE](LICENSE)). Third-party components
keep their own licenses; see [THIRD_PARTY_LICENSE.txt](THIRD_PARTY_LICENSE.txt). Any build
you distribute must carry those notices.
