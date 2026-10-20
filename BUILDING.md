# Local build

The game executable is generated locally from the user's game. The repository does not ship a precompiled translation of the game.

Prerequisites: Windows x64, Visual Studio C++ Build Tools and Windows SDK, Python 3.11+, LLVM/Clang 21, and a Release build of [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) at commit `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, including its submodules and compile_commands.json. Use a Visual Studio developer terminal. No proprietary Xbox development SDK is needed for this recomp pipeline.

Build the SDK's runtime and codegen targets with its documented CMake workflow. The SDK source checkout is read-only to our scripts: patches are applied to isolated copies in the private build directory's `overlay` folder. By default that directory is `.DWG2Recomp-local/<installation-id>` beside the source repository; an optional `work` path in the local profile can select another private location outside the repository. The runtime must have its Release static dependencies and object files available. Rebuild cached headers if Visual Studio has changed since the SDK build.

Create an **untracked** `toolchain.local.json` beside the launcher:

```json
{
  "python": "C:/path/to/python.exe",
  "clang": "C:/path/to/clang++.exe",
  "sdk_source": "C:/path/to/rexglue-sdk",
  "sdk_build": "C:/path/to/rexglue-release-build",
  "generator": "C:/path/to/dwg2-generator.exe"
}
```

`src/generator.cpp` is our original bounded codegen driver. Build it with the pinned SDK codegen executable's compile/link flags, substituting this source for its main source. It uses `CodegenPipeline::Create`, verifies the expected image base, entry point and image size, then runs analysis and generation. A missing generator is reported before the game build; imported content is retained for a retry.

Build the launcher using `powershell -NoProfile -File scripts/build-launcher.ps1`, then run it. Its first successful import invokes `scripts/build-game.ps1`. To rebuild an installed game after changing source:

```powershell
./scripts/build-game.ps1 -GameData ./game-data -Output ./bin
```

Generated game code, input copies and build objects stay in the private build directory outside the repository. Output binaries stay in the local installation. Do not commit them or run this pipeline on GitHub Actions with game inputs. Runtime and launcher updates can be built without reimporting game data; versioned packages contain only changed host files and a SHA-256 manifest.

## Validation boundary

Local import and window-control checks are recorded privately. Extended mission gameplay, audio longevity, physical DualSense USB/Bluetooth/hotplug, save/reload under the new UI, and 2x/3x performance still require validation before a stable release. A window surviving startup is not proof of those properties.
