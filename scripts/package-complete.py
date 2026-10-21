"""Maintainer: package a player-only ZIP with compressed temporary preparation tools."""
from pathlib import Path
import argparse,hashlib,json,subprocess,shutil,zipfile

def digest(f):
 h=hashlib.sha256()
 with f.open('rb') as stream:
  for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
 return h.hexdigest()

def excluded(n):
 # These have no role in the fixed Windows x64 Release preparation recipe.
 return ('/lib/clang/23/lib/linux/' in '/'+n or n.endswith('.idl') or
         n.startswith('toolchain/x86_64-w64-mingw32/bin/') or
         n in ['codegen/rexruntime.dll','codegen/libc++.dll','codegen/libunwind.dll','codegen/libwinpthread-1.dll',
               'toolchain/bin/libc++.dll','toolchain/bin/libunwind.dll','toolchain/bin/libwinpthread-1.dll'])

def main():
 p=argparse.ArgumentParser();p.add_argument('--version',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--portable',type=Path,required=True);p.add_argument('--host-source-relink',type=Path);a=p.parse_args()
 repo=Path(__file__).resolve().parents[1];out=a.output.resolve();portable=a.portable.resolve()
 if out.exists() or out.is_relative_to(repo) or out.is_relative_to(portable):raise ValueError('Choose a new private output folder outside source/input trees')
 if 'Version = "'+a.version+'"' not in (repo/'launcher/Launcher.cs').read_text():raise ValueError('Launcher version differs')
 recipe=json.loads((portable/'build.json').read_text());kept=[];removed=[]
 for item in recipe['files']:
  n=item['path'];f=(portable/n).resolve()
  if not f.is_relative_to(portable) or digest(f)!=item['sha256']:raise ValueError('Portable hash mismatch: '+n)
  (removed if excluded(n) else kept).append(item)
 recipe['files']=kept
 # buildId represents native code/ABI, unchanged by repacking tools or removing unused files.
 out.mkdir(parents=True);stage=out/'player';stage.mkdir();prep=stage/'preparation';prep.mkdir()
 data=json.dumps(recipe,indent=2).encode();(prep/'build.json').write_bytes(data)
 expanded=len(data)
 with zipfile.ZipFile(prep/'tools.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  z.writestr('build.json',data)
  for item in kept:
   f=portable/item['path'];z.write(f,item['path']);expanded+=f.stat().st_size
 (prep/'package.json').write_text(json.dumps({'format':1,'sha256':digest(prep/'tools.zip'),'expandedBytes':expanded},indent=2))
 subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(repo/'scripts/build-launcher.ps1'),'-Output',str(stage/'START_DWG2.exe')],check=True)
 for n in ['READ_ME_FIRST.txt','START_HERE.txt','THIRD_PARTY_LICENSE.txt','TEST_CHECKLIST.md']:shutil.copyfile(repo/n,stage/n)
 for n in ['apply-update.ps1','apply-portable-update.ps1']:shutil.copyfile(repo/'scripts'/n,stage/n)
 payload=[f.relative_to(stage).as_posix() for f in sorted(stage.rglob('*')) if f.is_file()]
 (stage/'update.json').write_text(json.dumps({'version':a.version,'portableBuild':True,'compressedPreparation':True,'files':[{'path':n,'sha256':digest(stage/n)} for n in payload]},indent=2))
 commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
 (stage/'release.json').write_text(json.dumps({'version':a.version,'sourceCommit':commit,'gameFilesIncluded':False,'separateToolchainRequired':False,'compressedPreparation':True,'portableBuildId':recipe['buildId']},indent=2))
 (stage/'SHA256SUMS.txt').write_text(''.join(digest(f)+'  '+f.relative_to(stage).as_posix()+'\n' for f in sorted(stage.rglob('*')) if f.is_file()))
 dest=out/('DWG2Recomp-v02-alpha-player.zip')
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for f in sorted(stage.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(stage).as_posix(),compress_type=zipfile.ZIP_STORED if f.name=='tools.zip' else zipfile.ZIP_DEFLATED)
 if a.host_source_relink:shutil.copyfile(a.host_source_relink,out/'DWG2Recomp-v02-alpha-developer-source-relink.zip')
 result={'zip':str(dest),'bytes':dest.stat().st_size,'sha256':digest(dest),'playerFiles':sum(f.is_file() for f in stage.rglob('*')),'expandedPlayerBytes':sum(f.stat().st_size for f in stage.rglob('*') if f.is_file()),'temporaryToolsBytes':expanded,'removedPreparationFiles':len(removed),'keptPreparationFiles':len(kept)}
 (out/'package-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)

if __name__=='__main__':main()
