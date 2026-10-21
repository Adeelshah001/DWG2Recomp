"""Build one game-free ZIP from reviewed sources and a verified portable capsule."""
from pathlib import Path
import argparse,hashlib,json,subprocess,shutil,zipfile

def digest(f):
 h=hashlib.sha256()
 with f.open('rb') as stream:
  for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
 return h.hexdigest()

def main():
 p=argparse.ArgumentParser();p.add_argument('--version',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--portable',type=Path,required=True);p.add_argument('--host-source-relink',type=Path,required=True);a=p.parse_args()
 repo=Path(__file__).resolve().parents[1];out=a.output.resolve();portable=a.portable.resolve()
 if out.exists() or out.is_relative_to(repo) or out.is_relative_to(portable):raise ValueError('Choose a new private output folder outside source/input trees')
 if 'Version = "'+a.version+'"' not in (repo/'launcher/Launcher.cs').read_text():raise ValueError('Launcher version differs')
 recipe=json.loads((portable/'build.json').read_text())
 expected={x['path']:x['sha256'] for x in recipe['files']};actual={f.relative_to(portable).as_posix() for f in portable.rglob('*') if f.is_file() and f.name!='build.json'}
 if actual!=set(expected):raise ValueError('Portable inventory differs from recipe')
 for n,h in expected.items():
  f=(portable/n).resolve()
  if not f.is_relative_to(portable) or digest(f)!=h:raise ValueError('Portable hash mismatch: '+n)
 out.mkdir(parents=True);stage=out/'complete';stage.mkdir()
 names=['.gitattributes','.gitignore','README.md','BUILDING.md','THIRD_PARTY_LICENSE.txt','READ_ME_FIRST.txt','START_HERE.txt','RELEASE_GUIDE.md','WISHLIST.md','TEST_CHECKLIST.md']
 names += [f.relative_to(repo).as_posix() for d in ['src','launcher','config','scripts'] for f in (repo/d).rglob('*') if f.is_file() and f.suffix in ('.cpp','.h','.inc','.patch','.cs','.json','.toml','.py','.ps1')]
 for n in names:
  f=repo/n
  if f.is_symlink() or n.endswith('toolchain.local.json'):raise ValueError('Unexpected source file')
  dest=stage/n;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,dest)
 subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(repo/'scripts/build-launcher.ps1'),'-Output',str(stage/'START_DWG2.exe')],check=True)
 shutil.copytree(portable,stage/'portable');(stage/'developer').mkdir();shutil.copyfile(a.host_source_relink,stage/'developer/host-source-relink.zip')
 for n in ['apply-update.ps1','apply-portable-update.ps1']:shutil.copyfile(repo/'scripts'/n,stage/n)
 payload=['START_DWG2.exe','READ_ME_FIRST.txt','START_HERE.txt','THIRD_PARTY_LICENSE.txt','TEST_CHECKLIST.md','apply-update.ps1','apply-portable-update.ps1']
 payload += ['portable/'+f.relative_to(portable).as_posix() for f in sorted(portable.rglob('*')) if f.is_file()]
 (stage/'update.json').write_text(json.dumps({'version':a.version,'portableBuild':True,'files':[{'path':n,'sha256':digest(stage/n)} for n in payload]},indent=2))
 commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
 (stage/'release.json').write_text(json.dumps({'version':a.version,'sourceCommit':commit,'gameFilesIncluded':False,'separateToolchainRequired':False,'portableBuildId':recipe['buildId']},indent=2))
 (stage/'SHA256SUMS.txt').write_text(''.join(digest(f)+'  '+f.relative_to(stage).as_posix()+'\n' for f in sorted(stage.rglob('*')) if f.is_file()))
 dest=out/('DWG2Recomp-v'+a.version+'-complete.zip')
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for f in sorted(stage.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(stage).as_posix())
 (out/'SHA256SUMS.txt').write_text(digest(dest)+'  '+dest.name+'\n')
 print(json.dumps({'zip':str(dest),'bytes':dest.stat().st_size,'sha256':digest(dest)}))

if __name__=='__main__':main()
