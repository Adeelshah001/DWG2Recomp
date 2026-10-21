# Publish a release yourself

## Upload a prepared complete package

1. Sign into GitHub and open https://github.com/Adeelshah001/DWG2Recomp/releases.
2. Click **Draft a new release**.
3. Choose a new version tag (for example `v0.2.2-alpha`) and the reviewed source commit on main. Never move an existing published tag.
4. Enter a release title and describe changes, known limitations and validation. Do not claim stable 60 FPS without gameplay testing.
5. Attach **one complete ZIP** containing the launcher, portable preparation tools, docs, updater and nested developer source/relink archive. Do not attach separate update/source packages. GitHub's automatic source-code archives are normal and separate from uploaded assets.
6. Put the complete ZIP SHA-256 in the release notes. Check the uploaded filename and size. Mark the release **pre-release** while it is alpha.
7. Publish, reopen the page, and confirm there is exactly one uploaded ZIP. Download it and check its SHA-256 before sharing the release link.

Never upload an installed game folder. Exclude ISO/XEX, extracted assets, generated title C++/objects/executable, saves, controls.ini, display.ini, profiles, logs, local paths, credentials and proprietary SDKs.

## Assemble a new package from reviewed build material

1. Update the version in launcher/Launcher.cs and the user docs. Commit reviewed source changes to main.
2. Prepare the verified portable capsule and matching open-source host source/relink ZIP (BUILDING.md). Keep build output outside the repository.
3. Run in PowerShell, replacing the paths:

   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -Version 0.2.1-alpha -OutputFolder C:\Releases\DWG2-unique -PortableFolder C:\Private\portable -HostSourceRelink C:\Private\host-source-relink.zip`

4. Audit every outer/nested ZIP entry. Verify all preparation hashes, bundled DLL dependencies, matching host-only source/relink material and third-party notices. Test fresh ISO preparation and repeat reuse with external development tools removed from PATH; do not launch gameplay while that testing is paused.
5. Test the updater against a disposable installation with save/settings sentinels and a deliberately tampered package. It must reject bad input before changing files.
6. Upload only `DWG2Recomp-vVERSION-complete.zip`. Keep build logs and evidence private. SHA256SUMS.txt outside the ZIP is a local reference; the content checksums are also inside the complete package.

Existing players extract the complete ZIP separately and run its root apply-update.ps1 against their existing folder, following START_HERE.txt. They do not need a second download or a separate toolchain. Automatic startup update checks are still on the wishlist.
