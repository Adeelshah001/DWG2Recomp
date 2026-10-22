# Local preview source

This source snapshot corresponds to the locally installed preview. The v01 and
v02 Alpha release assets remain unchanged; downloading v02 does not include
these later changes yet.

## Implemented

- Setup displays progress through completion.
- File > Restart requests a fresh game process through START_DWG2.exe.
  Save before restarting; a crash is reported rather than automatically retried.
- View > Field of View offers Default and Extended (+10% perspective angle).
  Changes apply live and are saved separately from the game's saves.
- Startup autoload selects occupied slot 01 through the original startup-only
  load operation. Structural checks reject unsupported headers and missing slots.
  The original data application and readiness/error paths remain. Manual
  save/load behavior and automatic saving have not been changed.
- The options controller diagram follows the last controller used. Keyboard
  activity does not replace a PS5 diagram. PlayStation glyphs and the PS5 diagram
  use CC0 artwork; see assets/input-prompts/LICENSE.txt.
- Button Prompts offers Auto, Xbox and PlayStation. HUD seam correction targets
  identified UI atlases; it does not change world-texture filtering.
- XMA context updates preserve consumer-owned fields under shared serialization.
- Crash logs capture the original native module and offset before rethrowing
  supported hardware exceptions.

## Verification and remaining work

The local preview passed native build/import/ABI checks, patch replay, isolated
restart tests, original-function FOV/startup comparisons, audio concurrency
checks and GPU atlas/live-integer-resolution checks. Original game functions
and inputs used in private comparison tests are not distributed here.

Full gameplay, physical controller operation, audible music recovery and HUD
appearance still need manual validation. The reported background C000001D crash
is unresolved: the available dumps contain the rethrown exception, so the new
original-fault log is needed to identify its cause.

True 1.5x rendering is not available. Rational viewport/target foundations are
present, but production resolve ownership, texture routing and live migration
remain incomplete. Existing player scales remain 1x/2x/3x.

Gameplay/FPS testing and decompilation remain paused. No game data, generated
game source/executable, personal settings, save files, logs or crash dumps are
included in this repository.
