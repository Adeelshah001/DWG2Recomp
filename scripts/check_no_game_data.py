#!/usr/bin/env python3
"""CI gate: reject game content, generated code, binaries and personal paths in the repository."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Extensions and patterns that indicate game content / generated artifacts / binaries
FORBIDDEN_SUFFIXES = {
    '.iso', '.xex', '.xexp', '.elf',
    '.pdb', '.obj', '.pch',
    '.exe', '.dll', '.lib', '.a', '.o',
    '.zip',
}
FORBIDDEN_PREFIXES = (
    '_recomp.', '_funcs.', '_init.', '_register.', 'shader',
)

# Personal / absolute paths that must not appear in source files
PERSONAL_PATH_PATTERN = re.compile(
    r'(C:\\\\Projects|C:/Projects|/mnt/c/|\\\\Users\\\\|/Users/|riser|AppData)',
    re.IGNORECASE,
)

# Allowlisted files that may have personal paths (docs/scripts containing detection patterns)
ALLOWLIST = frozenset({
    'AGENTS.md',
    'scripts/check_no_game_data.py',
})

# Allowlisted binary assets
ALLOWED_BINARY = frozenset({
    'assets/logo.png',
    'assets/features/fov.gif',
    'assets/features/graphics-options.png',
    'assets/features/quit-game.png',
})

def is_forbidden_file(path: Path) -> str | None:
    name = path.name
    for suf in FORBIDDEN_SUFFIXES:
        if name.endswith(suf):
            return f'forbidden extension {suf}'
    for pre in FORBIDDEN_PREFIXES:
        if name.startswith(pre):
            return f'forbidden prefix {pre}'
    if path.suffix.lower() in ('.png', '.gif', '.ico', '.jpg', '.jpeg'):
        rel = path.relative_to(ROOT).as_posix()
        if rel not in ALLOWED_BINARY:
            return 'binary asset not in allowlist'
    return None

def scan_text_file(path: Path) -> list[tuple[int, str]]:
    """Return list of (line_number, offending_line) for personal paths."""
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return []
    hits = []
    for i, line in enumerate(text.splitlines(), 1):
        if PERSONAL_PATH_PATTERN.search(line):
            hits.append((i, line.strip()[:160]))
    return hits

def main() -> int:
    errors = 0
    for p in ROOT.rglob('*'):
        if not p.is_file():
            continue
        if '.git' in p.parts:
            continue
        rel = p.relative_to(ROOT).as_posix()
        if rel in ALLOWLIST:
            continue

        # Check file itself
        reason = is_forbidden_file(p)
        if reason:
            print(f'ERROR: {rel}: {reason}')
            errors += 1

        # Check text content for personal paths
        if p.suffix.lower() not in ('.png', '.gif', '.ico', '.jpg', '.jpeg', '.zip', '.pdb', '.obj', '.pch', '.exe', '.dll', '.lib', '.a', '.o'):
            hits = scan_text_file(p)
            for line_no, line in hits:
                print(f'ERROR: {rel}:{line_no}: personal/absolute path detected: {line}')
                errors += 1

    if errors:
        print(f'\n{errors} violation(s) found. Repository must not contain game content, binaries, or personal paths.')
        return 1
    print('OK: no game content, binaries or personal paths detected.')
    return 0

if __name__ == '__main__':
    sys.exit(main())