# Publish or update v02 Alpha

The repository has two release entries: v01 Alpha and v02 Alpha. Update the existing v02 release for this player-package revision; preserve v01. Existing Git tags retain their history. Record the exact source commit for replacement assets in the release notes and release.json.

## Upload the prepared player package

1. Sign into GitHub and open https://github.com/Adeelshah001/DWG2Recomp/releases/tag/v0.2-alpha.
2. Choose **Edit**. Keep the title **v02 Alpha** and mark it as a pre-release.
3. Attach `DWG2Recomp-v02-alpha-player.zip`. This is the only download players need; it contains the launcher, compressed preparation payload, updater, brief instructions and notices.
4. Attach the optional `DWG2Recomp-v02-alpha-developer-source-relink.zip` for matching open-source host source/relink material. Label it developer-only in the release notes. Do not nest it inside the player ZIP.
5. Include each archive's SHA-256, player ZIP size, source commit, validation and limitations. Do not claim stable 60 FPS without gameplay evidence.
6. Verify the new uploaded hashes before removing superseded v02 assets. Save old asset metadata and retain local copies. GitHub's automatic source archives are separate from uploaded assets.
7. Remove the superseded v0.2.1-alpha release entry only after v02 replacement assets and matching source are verified. Keep exactly the two requested release entries. Removing a release need not delete its Git tag.

Never upload an installed game folder. Exclude ISO/XEX, extracted assets, generated title C++/objects/executable, saves, settings, profiles, logs, local build paths, credentials and proprietary SDKs. Developer relink inputs contain host objects only.

## Assemble a reviewed package

1. Commit reviewed launcher/packaging changes to main. Keep the displayed version 0.2-alpha for the requested v02 release.
2. Keep the verified original portable capsule and matching host-source/relink archive outside Git. Follow BUILDING.md for maintainer builds.
3. Run PowerShell with your own paths:

   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -Version 0.2-alpha -OutputFolder C:\Releases\DWG2-unique -PortableFolder C:\Private\portable -HostSourceRelink C:\Private\host-source-relink.zip`

4. The assembler removes Windows-irrelevant Linux runtimes, IDL files and redundant DLL copies. Runtime DLLs are found through a controlled preparation PATH. It records hashes in preparation/build.json and compresses tools into preparation/tools.zip.
5. Test the player ZIP with a fresh legal ISO and a Windows-System32-only external PATH. Confirm successful preparation cleans expanded tools, and repeat launch reuses the executable. Keep gameplay/FPS testing paused while that remains the user's preference.
6. Test corrupted, incomplete and escaping archives, failed tool cleanup, update preservation/tamper rejection and unknown-file protection during old-tools cleanup. Audit the player and developer inventories and all checksums.
7. Upload the player ZIP and optional developer source/relink ZIP to the existing v02 release. Build logs and evidence stay private.

Existing players extract the player ZIP separately and run its apply-update.ps1 against their installation. Compatible prepared executables are reused; older ABI-incompatible installations prepare once. The updater preserves game data, saves and settings and removes a previous expanded portable folder only when every file belongs to its verified inventory. Unknown or modified files are retained. Automatic startup update checks are still planned.
