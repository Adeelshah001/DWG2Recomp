"""Maintainer helper: copy a pinned SDK and apply host/portable source patches.

Players do not run this script and need no Python, Git, CMake or Visual Studio.
"""
from pathlib import Path
import argparse,subprocess,shutil,importlib.util,json

def main():
 p=argparse.ArgumentParser();p.add_argument('--sdk-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
 repo=Path(__file__).resolve().parents[1];sdk=args.sdk_source.resolve();out=args.output.resolve()
 if out.exists() or out.is_relative_to(sdk) or out.is_relative_to(repo):raise ValueError('Choose a new private output directory outside both source trees')
 expected=json.loads((repo/'config/compiler.json').read_text())['sdkRevision']
 actual=subprocess.check_output(['git','-C',str(sdk),'rev-parse','HEAD'],text=True).strip()
 if actual!=expected:raise ValueError('SDK revision differs from the pinned recipe')
 shutil.copytree(sdk,out,ignore=shutil.ignore_patterns('.git','out','build','__pycache__'))
 spec=importlib.util.spec_from_file_location('local_build',repo/'scripts/build-local.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 patches=[*sorted((repo/'src').glob('*.patch')),*sorted((repo/'src/portable-sdk').glob('*.patch'))]
 for patch in patches:
  text=patch.read_text();relative=text.splitlines()[1][6:];target=out/relative
  if not target.resolve().is_relative_to(out):raise ValueError('Patch path escapes SDK copy')
  target.parent.mkdir(parents=True,exist_ok=True)
  target.write_text(m.apply_patch(target.read_text() if target.exists() else '',text))
 for name in ['input_config.h','input_host.h','input_ui.h','window_controls.h','dwg2_metrics.h','resolution_control.h','resolved_ranges.h','d3d12_live_resolution.inc','xma_synchronization.h','prompt_control.h','ui_atlas.h','input_art.h','input_art_decode.h','ui_atlas_fingerprints.h','d3d12_ui_atlas.inc','render_scale.h','scene_scale.h','camera_control.h','startup_load.h']:
  shutil.copyfile(repo/'src'/name,out/'include'/name)
 shutil.copytree(repo/'src',out/'dwg2-host/src')
 print('Patched private SDK:',out)

if __name__=='__main__':main()
