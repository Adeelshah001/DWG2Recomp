# How to post a DWG2Recomp release yourself

Repository: https://github.com/Adeelshah001/DWG2Recomp

The v0.1 alpha is a development release. A fresh installation needs the local
toolchain described in BUILDING.md to generate its executable from the user's
game. The update ZIP is for an already installed compatible build.

## 1. Prepare the release files

1. Close the game and launcher. Test the exact launcher/runtime you plan to ship.
2. Keep the working installation separate from release staging. Never ZIP the
   entire project folder or installation: it contains private game files.
3. Update the version in `launcher/Launcher.cs` and the release documentation.
   Run `scripts/package-release.ps1` with the version and a new output folder.
   It builds the launcher and copies only explicitly allowed source and
   documentation. It does not copy your installation or redistribute the runtime.
   For this alpha, from the repository folder:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -Version 0.1-alpha -OutputFolder 'C:\Releases\DWG2-v0.1-alpha'
   ```
4. Keep the full launcher ZIP, the update ZIP and `SHA256SUMS.txt` together.
   Check the archive file lists: no ISO, XEX, game assets, generated game C++,
   `dwg2.exe`, saves, cache, logs, local toolchain profile or proprietary SDK.
5. Extract the update ZIP to a temporary folder and test it against a disposable
   compatible installation. Confirm that game data, saves and display settings
   remain intact. Do not test a new update on your only copy of a save.
6. Write short release notes describing changes, installation, update instructions,
   known issues and what was actually tested. Do not promise stable 60 FPS
   without representative mission testing.

## 2. Commit the source changes

1. In GitHub Desktop or your Git client, review the changed-file list and diffs.
2. Commit only the reviewed launcher/source/configuration/documentation changes.
   The supplied `.gitignore` keeps private outputs out; do not force-add them.
3. Push the commit to `main`. Note its commit hash so the release points to the
   same source used to build the package.

## 3. Create the release on GitHub

1. Sign in to GitHub and open the repository above.
2. Click **Releases**, then **Draft a new release**.
3. Choose a new tag, for example `v0.1-alpha` for the first alpha or
   `v0.1.1-alpha` for the next update. Create that tag on the tested commit.
   Use a new version instead of replacing an already published release.
4. Set a clear title, such as **v0.1 Alpha**.
5. Paste your release notes into the description. Say which ZIP is for a fresh
   local build and which is an update for an existing installation.
6. Drag the two ZIPs and `SHA256SUMS.txt` into the release asset area. You can
   attach this guide separately too. Wait for every upload to finish.
7. Select **Set as a pre-release** for alpha builds. Keep the repository private
   if that is your intended audience; a release does not change its visibility.
8. Save a draft if you need to review it. When ready, click **Publish release**.

## 4. Verify what other users receive

1. Open the published release page and download its assets again.
2. Compare each ZIP's SHA-256 hash with `SHA256SUMS.txt`. In Windows PowerShell:

   ```powershell
   Get-FileHash -Algorithm SHA256 -LiteralPath '.\DWG2Recomp-v0.1-alpha-windows-x64.zip'
   ```

3. Extract the downloaded package and follow its README. Confirm the version
   and update instructions match the release notes.
4. Share the release page URL. People need repository access while it is private.

## Installing an update

Close the game and launcher, extract the update ZIP, then run from that folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\apply-update.ps1 -GameFolder 'C:\path\to\installed-game' -Package .
```

Choose the folder containing `START_DWG2.exe` and `bin`, not the `bin` folder.
The updater checks compatibility and file hashes and creates a local rollback
backup before replacing the allowed host files. Keep your normal save backup.

Official GitHub instructions:
https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository
