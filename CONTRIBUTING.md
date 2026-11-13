# Contributing

Thanks for taking a look. This is an unofficial fan recompilation; contributions of bug
reports, fixes and features are welcome, and the repository is deliberately laid out so a
person and their AI assistant can work in it.

## Scope

The project is a **recompilation plus features**: the launcher, the host layer, the
renderer/audio/input work and the in-game menus. Byte-exact source recovery is not part of
this project, so please do not send reconstructed guest routines or "matches the original
instructions" patches.

## Ground rules

1. **No game content, ever.** No ISO, `default.xex`, extracted assets, generated title
   code (`*_recomp.*`, `*_funcs.*`), compiled objects, caches, saves, logs or screenshots
   that reproduce copyrighted art. CI rejects the obvious cases; do not test the limit.
2. **Do not break saves, settings or updates.** Existing player saves, `settings.ini`,
   `user-data/`, `bin/` and rollback material must survive. A save that fails must never
   be reported as successful.
3. **Do not make players recompile the game for a small change.** Adding a new path under
   `include/` or `headers/` in the portable preparation invalidates every player's cache.
4. **Say what you actually tested.** "Builds cleanly" is not gameplay verification. If you
   could not test on real hardware, say so in the pull request.
5. **One change per pull request**, with a short imperative commit message.

## Reporting a bug

Open an issue with:

- the launcher version shown in **Help > About** (or `maintenance/release.json`);
- Windows version, CPU, GPU and driver version, RAM;
- the graphics settings you were using (resolution scale, FOV, fullscreen, VSync);
- exact reproduction steps, expected behaviour and actual behaviour;
- optionally a screenshot or video of the problem.

Do **not** attach caches, generated files, imported game data or your whole installation.
If a log matters, paste the relevant lines. Review anything you post for personal
information first.

Save-damage reports are the most valuable thing you can send. Keep the affected save
untouched (copy it aside), note whether the game was saving when it failed, and describe
what the game showed afterwards.

## Working on code

1. Fork and clone.
2. Read [BUILDING.md](BUILDING.md) and pick the smallest path that exercises your change:
   launcher-only changes need no SDK; host-layer changes need the full local runtime.
3. Keep generated output outside the checkout.
4. Run `python scripts/check_no_game_data.py` and `git diff --check` before committing.
5. Describe in the pull request: what changed, why, how you built it, what you tested in
   game (mission, menu, save/relaunch) and what you could not test.

### Where changes belong

| Change | Location |
| --- | --- |
| Setup, ISO import, menus, controls, updates, shortcuts | `launcher/` |
| Host hooks, in-game menu/graphics behaviour, save handling | `src/` (`main.cpp` and the `*.inc`/`*.h` it includes) |
| Engine, renderer, audio, input or SDK behaviour | `src/*.patch`, `src/portable-sdk/*.patch` |
| Build, patch or update tooling | `scripts/` |
| Documentation | `README.md`, `BUILDING.md`, `docs/` |

Patches are exact-context unified diffs against the pinned SDK revision. Keep the context
lines intact; a patch that only applies to your machine is not mergeable.

### Style

- C# and C++ follow the surrounding files: four-space indentation, braces on the same
  line, no dependency additions.
- Python uses the standard library only, so a contributor needs nothing beyond Python.
- PowerShell must work in Windows PowerShell 5.1; do not use PowerShell 7 syntax.
- Keep comments about *why*, not *what*. Existing comments explain constraints (cache
  keys, save ordering, ABI) — preserve them.

### Using AI assistants

Using an AI assistant is fine and this project was itself written that way. Two
conditions: disclose it in the pull request, and personally verify the build and the
in-game behaviour you claim. Generated code that has never been compiled or run will be
sent back.

`AGENTS.md` exists so an assistant can start with the right constraints instead of
guessing. Point yours at it.

## What gets rejected

- Game files, assets, generated code, binaries, caches or saves in the diff.
- Changes that quietly alter save semantics, updater allow-lists or the preparation cache
  key without discussion.
- Reconstructed original game routines.
- "Performance improved" claims without measurements, or "fixed" claims without a
  reproduction.

## License

By contributing you agree that your contribution is licensed under GPL-3.0, the license of
this project (see [LICENSE](LICENSE)). Third-party code keeps its own license and notices
in [THIRD_PARTY_LICENSE.txt](THIRD_PARTY_LICENSE.txt); add an entry there if you introduce
a new component.
