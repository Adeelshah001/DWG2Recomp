# Publish an alpha player release

1. Build and review the matching launcher/runtime/host source and private
   preparation capsule. Exclude game inputs, generated title code/executable,
   saves, settings, logs and the local project plan.
2. Package one `DWG2Recomp-vNN-alpha-player.zip`. Include the launcher,
   compressed preparation tools, updater scripts, instructions and notices.
   Keep optional developer host source/relink material separate.
3. Create the corresponding `v0.N-alpha` GitHub release, mark it as an alpha
   prerelease, and upload the player ZIP. Preserve older releases.
4. Record source commit, package SHA256, size, tested behavior and limitations.
   Verify GitHub's uploaded asset digest matches the local SHA256.
5. Keep the repository/releases public for anonymous launcher update checks.
   The updater reads the releases list, including alpha prereleases. It requires
   one compatible `DWG2Recomp-*-player.zip` with a GitHub SHA256 asset digest.
6. For the planned v03-to-v04 test, publish v04 only when requested. A newer
   version triggers a prompt; equal/older versions are ignored. Declining must
   keep the current game running; accepting must install only after it closes.

v03 introduces the automatic updater, so v02 users first install v03 manually.
Never publish an installed game folder or include a shared GitHub credential.
