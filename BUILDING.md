# Building and preparing

Players use the player ZIP: run START_DWG2.exe and select their own supported ISO. No development tools need to be installed. The included portable compiler is invoked automatically, offline. Game-derived source and executable are generated only on the user's computer.

## Maintainers

The portable runtime uses LLVM-MinGW 20261006 UCRT x86_64 (LLVM 23.1.3), and ReXGlue SDK revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. Do not mix this GNU C++ ABI with older MSVC host libraries or game executables.

Official toolchain: https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006

Archive SHA-256: `317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a`.

1. Obtain the pinned SDK and its submodules in a private work folder. Keep game inputs outside Git.
2. Run `python scripts/prepare-portable-sdk.py --sdk-source <checkout> --output <new-private-sdk>`. This applies the existing controls/resolution patches and portable compiler fixes and copies the original host source. No game input is needed for the host.
3. Configure Release with CMake/Ninja and LLVM-MinGW clang/clang++, targeting x86_64-w64-mingw32. Disable Tracy; enable the D3D12 backend. The matching source snapshot and CMake configuration are included in the release's optional developer source/relink archive. Build rexruntime, rexgpu-xenos and dwg2-generator.
4. Run `python scripts/assemble-portable.py --sdk-source <patched-sdk> --sdk-build <build> --toolchain <llvm-mingw> --host-library <libdwg2-host.a> --flags <portable-compile-flags.json> --output <new-private-portable>`. The release developer archive contains the original host bootstrap sources/build commands and host-only relink inputs. Build the bootstrap from src/main.cpp plus SDK src/ui/rex_app.cpp and windowed_app_main_sdl.cpp; do not include generated title code. The archive must define no sub_* guest functions.
5. Build the .NET Framework launcher with `scripts/build-launcher.ps1`; its compiler is part of Windows .NET Framework. Players do not execute maintainer scripts.
6. Create the one player ZIP using `scripts/package-release.ps1` as described in RELEASE_GUIDE.md. Validate it with a fresh ISO import and a repeated no-launch preparation before publishing.

The previous MSVC developer scripts/configuration remain for historical reproducibility. They are not the player preparation path for v02 Alpha and cannot be used to relink the portable GNU runtime.

The optional developer host source/relink archive contains no game objects. Relink the two host DLLs using its response files and the bundled portable compiler, or rebuild the matching source with CMake/Ninja. Keep runtime and GPU plugin built from the same configuration. Modifying host libraries is permitted; do not distribute game-derived objects, generated title code or executable.

Player v02 packages use preparation/build.json, preparation/package.json and preparation/tools.zip. Hash-verified temporary extraction is removed after preparation, including on failure. Removing irrelevant Linux/IDL files and duplicate loader copies does not change the guest ABI/buildId. Maintainer source/relink material is an optional separate release asset.
