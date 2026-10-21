"""Maintainer: assemble game-free portable preparation tools from reviewed host outputs."""
from pathlib import Path
import shutil,json,hashlib,subprocess,os,re
import argparse
p=argparse.ArgumentParser()
for n in ['sdk-source','sdk-build','toolchain','host-library','flags','output']:p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();S=Path(__file__).resolve().parents[1];SDK=a.sdk_source.resolve();B=a.sdk_build.resolve();TC=a.toolchain.resolve();O=SDK/'out/win-amd64';PORT=a.output.resolve()
if PORT.exists() or any(PORT.is_relative_to(x) for x in [S,SDK,B,TC]):raise ValueError('Choose a new private output directory')
PORT.mkdir(parents=True)

def copy(f,n):
 out=PORT/n;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,out)
# Only the compiler, linker and x64 target/resources used by first-run preparation.
bins={'clang.exe','clang++.exe','clang-23.exe','ld.lld.exe','ld.exe','lld.exe','lld-link.exe','x86_64-w64-mingw32-clang.exe','x86_64-w64-mingw32-clang++.exe','x86_64-w64-mingw32-ld.exe'}
exclude_dll={'liblldb.dll','libpython3.dll','libpython3.14.dll','libomp.dll','libclang_rt.asan_dynamic-x86_64.dll'}
for f in TC.rglob('*'):
 if not f.is_file():continue
 n=f.relative_to(TC);parts=n.parts
 allowed=parts[0] in ('include','lib','x86_64-w64-mingw32') or n.name=='LICENSE.TXT'
 if parts[0]=='bin':allowed=n.name in bins or (f.suffix=='.dll' and n.name not in exclude_dll) or (f.suffix=='.cfg' and ('x86_64' in n.name or n.name=='mingw32-common.cfg'))
 if any(x in n.name for x in ['aarch64','armv7','i386','i686','arm64ec']):allowed=False
 if allowed:copy(f,'toolchain/'+n.as_posix())
for base,dest in [(SDK/'include','include'),(B/'include','include')]:
 for f in base.rglob('*'):
  if f.is_file():copy(f,dest+'/'+f.relative_to(base).as_posix())
for name in ['simde','fmt/include','spdlog/include','sdl3/include','tomlplusplus/include','imgui','dxc/include']:
 base=SDK/'thirdparty'/name
 for f in base.rglob('*'):
  if f.is_file() and f.suffix.lower() in ('.h','.hpp','.inc','.inl','.def','.natvis'):copy(f,'headers/'+name+'/'+f.relative_to(base).as_posix())
for f in (B/'thirdparty/sdl3/include-revision').rglob('*'):
 if f.is_file():copy(f,'headers/sdl-generated/'+f.relative_to(B/'thirdparty/sdl3/include-revision').as_posix())
copy(S/'config/dwg2.toml','config/dwg2.toml')
copy(a.host_library,'lib/libdwg2-host.a')
libs=['librexruntime.dll.a','libfmt.a','libspdlog.a','libSDL3.a','libxxhash.a','libdisasm.a']
for n in libs:copy(O/n,'lib/'+n)
runtime=['rexruntime.dll','rexgpu-xenos.dll','libc++.dll','libunwind.dll','libwinpthread-1.dll']
for n in runtime:copy((O if n.startswith('rex') else TC/'bin')/n,'runtime/'+n)
copy(O/'dwg2-generator.exe','codegen/dwg2-generator.exe')
# The codegen tool has its own app-local dependency search directory.
for n in runtime:
 if n!='rexgpu-xenos.dll':copy(PORT/'runtime'/n,'codegen/'+n)
# Flags use only {portable} and {generated} placeholders; no absolute machine paths.
result=json.loads(a.flags.read_text())
for flag in result:
 if re.search(r'[A-Za-z]:[\\/]',flag):raise ValueError('Absolute path in compile flags')
notices=PORT/'notices';notices.mkdir()
shutil.copyfile(TC/'python/share/libffi/LICENSE.txt',notices/'libffi-LICENSE.txt')
links=['{portable}/lib/libdwg2-host.a',*['{portable}/lib/'+n for n in libs],'-municode','-mwindows','-lcomctl32','-lws2_32','-ldbghelp','-ld3d12','-ldxgi','-ldxguid','-lole32','-loleaut32','-lwinmm','-limm32','-lversion','-luuid','-ladvapi32','-lsetupapi','-lshell32','-lhid','-ldinput8','-luser32','-lgdi32','-lkernel32']
files=[{'path':f.relative_to(PORT).as_posix(),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(PORT.rglob('*')) if f.is_file()]
build_id=hashlib.sha256(json.dumps({'files':files,'compile':result,'link':links},sort_keys=True).encode()).hexdigest()[:24]
recipe={'format':1,'buildId':build_id,'compiler':'toolchain/bin/clang++.exe','generator':'codegen/dwg2-generator.exe','compile':result,'link':links,'files':files,'runtimeFiles':[{'source':'runtime/'+n,'target':n} for n in runtime]}
(PORT/'build.json').write_text(json.dumps(recipe,indent=2))
print('Portable capsule:',PORT,'build',build_id)
