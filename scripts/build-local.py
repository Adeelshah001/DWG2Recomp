"""Local-only recompilation. Never uploads inputs, generated code or binaries."""
from pathlib import Path
import argparse, concurrent.futures, ctypes, hashlib, json, os, re, shutil, subprocess, threading

REPO = Path(__file__).resolve().parents[1]

def split_windows(command):
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    ptr = parse(command, ctypes.byref(count))
    try:
        return [ptr[i] for i in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ptr)

def apply_patch(source, patch):
    """Apply reviewed exact-context unified diffs to an isolated source copy."""
    original = source.splitlines(True)
    lines = patch.splitlines(True)
    result, cursor, i = [], 0, 2
    while i < len(lines):
        match = re.match(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@', lines[i])
        if not match:
            raise ValueError('Invalid source patch header')
        start = max(0, int(match.group(1)) - 1)
        if start < cursor:
            raise ValueError('Overlapping source patch')
        result.extend(original[cursor:start]); cursor = start; i += 1
        while i < len(lines) and not lines[i].startswith('@@ '):
            line = lines[i]; i += 1
            if line.startswith('\\'):
                continue
            if line[0] in ' -':
                if cursor >= len(original) or original[cursor] != line[1:]:
                    raise ValueError('Source patch context differs from the pinned SDK')
                cursor += 1
            if line[0] in ' +':
                result.append(line[1:])
    result.extend(original[cursor:])
    return ''.join(result)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--game-data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--engine-only', action='store_true')
    args = parser.parse_args()
    profile = json.loads(args.profile.read_text(encoding='utf-8-sig'))
    sdk, sdk_build = Path(profile['sdk_source']), Path(profile['sdk_build'])
    clang, generator = Path(profile['clang']), Path(profile['generator'])
    recipe = json.loads((REPO / 'config/compiler.json').read_text())
    revision = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != recipe['sdkRevision']:
        raise ValueError('SDK revision differs from the supported pinned build')
    xex = args.game_data.resolve() / 'default.xex'
    if hashlib.sha256(xex.read_bytes()).hexdigest() != '0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b':
        raise ValueError('Unsupported game executable')
    identifier = hashlib.sha256(str(REPO.resolve()).encode()).hexdigest()[:12]
    work = Path(profile.get('work', REPO.parent / '.DWG2Recomp-local' / identifier)).resolve()
    if work.is_relative_to(REPO.resolve()):
        raise ValueError('Choose a private build directory outside the source repository')
    for name in ['input', 'generated', 'objects', 'overlay', 'engine', 'logs']:
        (work / name).mkdir(parents=True, exist_ok=True)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    values = dict(sdk_source=sdk.as_posix(), sdk_build=sdk_build.as_posix(),
                  generated=(work/'generated').as_posix(), work=work.as_posix(),
                  overlay=(work/'engine').as_posix(), repo=REPO.as_posix())
    records = []
    record_lock = threading.Lock()
    environment = os.environ.copy()
    environment['PATH'] = os.pathsep.join([str(generator.parent),str(sdk/'out/win-amd64'),str(sdk_build/'artifacts'),environment.get('PATH','')])
    def run(name, command, cwd=sdk_build):
        print(name, flush=True)
        with (work/'logs'/(name+'.stdout')).open('w') as out, (work/'logs'/(name+'.stderr')).open('w') as err:
            child = subprocess.Popen(command, cwd=cwd, env=environment, stdout=out, stderr=err)
            record = dict(name=name, command=command, cwd=str(cwd), pid=child.pid, exitCode=None)
            with record_lock:
                records.append(record)
                (work/'build.json').write_text(json.dumps(records, indent=2))
            try:
                code = child.wait(timeout=600)
            except subprocess.TimeoutExpired:
                child.kill(); child.wait(); code = 'timeout'
            with record_lock:
                record['exitCode'] = code
                (work/'build.json').write_text(json.dumps(records, indent=2))
        if code:
            raise RuntimeError(name + ' failed; see ' + str(work/'logs'/(name+'.stderr')))
    def link(name, command):
        rsp = work / (name+'.rsp')
        rsp.write_text('\n'.join('"'+s.replace('\\','\\\\').replace('"','\\"')+'"' for s in command[1:]))
        run(name, [command[0], '@'+str(rsp)])
    # Header overlays affect only our isolated compilation, never the SDK checkout.
    for patch in sorted((REPO/'src').glob('*.h.patch')):
        text = patch.read_text()
        relative = text.splitlines()[0][6:]
        header = work / 'overlay' / relative
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(apply_patch((sdk/relative).read_text(), text))
    # Recompile original host overlays; never modify the SDK checkout.
    entries = json.loads((sdk_build/'compile_commands.json').read_text())
    for patch in sorted((REPO/'src').glob('*.cpp.patch')):
        text = patch.read_text()
        relative = text.splitlines()[0][6:]
        source = work / 'overlay' / patch.name[:-6]
        source.write_text(apply_patch((sdk/relative).read_text(), text))
        entry = next(e for e in entries if Path(e['file']).resolve() == (sdk/relative).resolve())
        command = split_windows(entry['command'])
        if '-include-pch' in command:
            i = command.index('-include-pch')
            if command[i-1] == '-Xclang': del command[i-1:i+3]
            else: del command[i:i+2]
        command[0] = str(clang)
        command[command.index('-c')+1] = str(source)
        command[command.index('-o')+1] = str(work/'engine'/(source.stem+'.obj'))
        command[1:1] = ['-I'+str(work/'overlay/include'), '-I'+str(REPO/'src'), '-I'+str((sdk/relative).parent)]
        run('engine-'+source.stem,command)
    engine_link = json.loads((REPO/'config/engine-link.json').read_text())
    command = [str(clang)] + [s.format(**values) for s in engine_link]
    command += ['-o',str(work/'engine/rexruntime.dll'),'-Xlinker','/implib:'+str(work/'engine/rexruntime.lib')]
    link('engine-link',command)
    gpu_recipe = REPO/'config/gpu-link.json'
    if gpu_recipe.is_file():
        command = [str(clang)] + [s.format(**values) for s in json.loads(gpu_recipe.read_text())]
        command += ['-o',str(work/'engine/rexgpu-xenos.dll'),'-Xlinker','/implib:'+str(work/'engine/rexgpu-xenos.lib')]
        link('gpu-link',command)
        shutil.copyfile(work/'engine/rexgpu-xenos.dll',output/'rexgpu-xenos.dll')
    else:
        shutil.copyfile(sdk_build/'artifacts/rexgpu-xenos.dll',output/'rexgpu-xenos.dll')
    shutil.copyfile(sdk_build/'artifacts/TracyClient.dll',output/'TracyClient.dll')
    shutil.copyfile(work/'engine/rexruntime.dll',output/'rexruntime.dll')
    if args.engine_only:
        return
    if not generator.is_file():
        raise FileNotFoundError('Build src/generator.cpp against the pinned codegen SDK and set generator in toolchain.local.json')
    shutil.copyfile(xex,work/'input/default.xex')
    shutil.copyfile(REPO/'config/dwg2.toml',work/'dwg2.toml')
    run('generate',[str(generator),str(work/'dwg2.toml')],cwd=work)
    base = [str(clang)] + [s.format(**values) for s in recipe['compile']]
    pch = work/'objects/dwg2.pch'
    run('game-pch',base+['-x','c++-header',str(work/'generated/dwg2_pch.h'),'-o',str(pch)])
    sources = sorted((work/'generated').glob('*.cpp')) + [REPO/'src/main.cpp',sdk/'src/ui/windowed_app_main_sdl.cpp',sdk/'src/ui/rex_app.cpp']
    if len(sources) < 4:
        raise RuntimeError('No generated translation units')
    def compile_source(item):
        index,source = item
        obj = work/'objects'/(str(index)+'-'+source.stem+'.obj')
        run('game-'+str(index),base+['-include-pch',str(pch),'-c',str(source),'-o',str(obj)])
        return obj
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        objects = list(pool.map(compile_source,enumerate(sources)))
    command = [str(clang)] + [str(o) for o in objects] + [s.format(**values) for s in recipe['link']]
    command += ['-o',str(work/'objects/dwg2.exe'),'-Xlinker','/implib:'+str(work/'objects/dwg2.lib')]
    link('game-link',command)
    shutil.copyfile(work/'objects/dwg2.exe',output/'dwg2.exe')
    print('Local game executable created. No game content was uploaded.',flush=True)

if __name__ == '__main__':
    main()
