"""Build libdwg2-host.a: the host layer that is linked with generated title code.

The host layer is this repository's own C++ (src/main.cpp) plus two SDK files
that own the window and app lifetime. It contains no title code: every sub_*
symbol comes from the generated translation units, which are built on the
machine that owns the game image.

Example (private folders outside the repository):

    python scripts/build-host-library.py ^
        --toolchain C:\\private\\llvm-mingw ^
        --sdk-source C:\\private\\dwg2-sdk ^
        --sdk-build C:\\private\\dwg2-sdk-build ^
        --output C:\\private\\out\\libdwg2-host.a
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]

# Kept in one place so a rebuild of the shipped host layer uses identical code
# generation. The definitions mirror config/compiler.json's host section.
DEFINES = [
    '-DREXGLUE_BUILD_CONFIG="Release"',
    '-DREX_PLATFORM_WINDOWS=1',
    '-DREX_HAS_D3D12=1',
    '-DWINVER=0x0A00',
    '-D_WIN32_WINNT=0x0A00',
    '-DNTDDI_VERSION=0x0A000002',
    '-DNOMINMAX',
    '-DWIN32_LEAN_AND_MEAN',
    '-DSPDLOG_FMT_EXTERNAL',
    '-DSPDLOG_COMPILED_LIB',
]
CODEGEN = [
    '-std=c++23',
    '-O3',
    '-DNDEBUG',
    '-fms-extensions',
    '-fno-char8_t',
    '-fno-strict-aliasing',
    '-ffp-model=strict',
    '-march=x86-64-v2',
    '-msse4.1',
]
# Relative include roots inside the prepared SDK, in the order the shipped
# host object was compiled with.
SDK_INCLUDES = [
    'include',
    'thirdparty/simde',
    'thirdparty/simde/simde',
    'thirdparty/fmt/include',
    'thirdparty/spdlog/include',
    'thirdparty/sdl3/include',
    'thirdparty/tomlplusplus/include',
    'thirdparty/imgui',
    'thirdparty/dxc/include',
]
SDK_BUILD_INCLUDES = ['include', 'thirdparty/sdl3/include-revision']
HOST_SOURCES = ['src/main.cpp']
SDK_SOURCES = ['src/ui/rex_app.cpp', 'src/ui/windowed_app_main_sdl.cpp']


def run(command):
    print(' '.join(str(part) for part in command), flush=True)
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True)
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.returncode:
        sys.stderr.write(result.stderr)
        raise SystemExit('command failed: ' + str(command[0]))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--toolchain', type=Path, required=True,
                        help='llvm-mingw root; clang++/llvm-ar are taken from bin/')
    parser.add_argument('--sdk-source', type=Path, required=True,
                        help='SDK source prepared by scripts/prepare-portable-sdk.py')
    parser.add_argument('--sdk-build', type=Path, required=True,
                        help='CMake build directory of that SDK (for generated headers)')
    parser.add_argument('--output', type=Path, required=True, help='libdwg2-host.a to write')
    parser.add_argument('--work', type=Path, help='object directory (default: alongside the output)')
    parser.add_argument('--generated', type=Path,
                        help='optional generated/ directory, only needed if a host header includes it')
    args = parser.parse_args()

    clang = args.toolchain.resolve() / 'bin' / 'clang++.exe'
    archiver = args.toolchain.resolve() / 'bin' / 'llvm-ar.exe'
    for tool in (clang, archiver):
        if not tool.is_file():
            raise SystemExit('missing tool: ' + str(tool))
    sdk, sdk_build = args.sdk_source.resolve(), args.sdk_build.resolve()
    for base in (sdk / 'include', sdk_build / 'include'):
        if not base.is_dir():
            raise SystemExit('missing SDK include directory: ' + str(base))
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    work = (args.work or output.parent / 'host-objects').resolve()
    if work.is_relative_to(REPO):
        raise SystemExit('choose a work directory outside this repository: generated objects are private')
    work.mkdir(parents=True, exist_ok=True)

    includes = ['-I' + str(sdk / part) for part in SDK_INCLUDES]
    includes += ['-I' + str(sdk_build / part) for part in SDK_BUILD_INCLUDES]
    if args.generated:
        includes.append('-I' + str(args.generated.resolve()))
    includes.append('-I' + str(REPO / 'src'))
    # Keep absolute build paths out of the object file so archives from
    # different machines stay comparable.
    prefix = ['-ffile-prefix-map=' + str(REPO / 'src') + '=dwg2-host/src']
    common = CODEGEN + DEFINES + includes + prefix + [
        '-include', 'windows.h',
        '-include', 'rex/platform/exceptions.h',
    ]

    objects = []
    for relative in HOST_SOURCES + SDK_SOURCES:
        source = (REPO / relative) if relative in HOST_SOURCES else (sdk / relative)
        if not source.is_file():
            raise SystemExit('missing source: ' + str(source))
        obj = work / (source.stem + '.obj')
        run([clang] + common + ['-c', str(source), '-o', str(obj)])
        objects.append(obj)
    if output.exists():
        output.unlink()
    run([archiver, 'rcs', str(output)] + objects)
    print(json.dumps({
        'archive': str(output),
        'bytes': output.stat().st_size,
        'objects': [str(obj) for obj in objects],
    }, indent=2))


if __name__ == '__main__':
    main()
