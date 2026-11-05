"""Build Store variants and stage immutable resources (run asset preparation first)."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = ['OutRun', 'PixelWheels', 'GoldMiner', 'OpenOMF', 'OpenTyrian', 'TouchTest']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('projects', nargs='*', help=', '.join(PROJECTS))
p.add_argument('--target', choices=['riscv', 'xtensa'], default='riscv')
p.add_argument('--no-aot', action='store_true', help='separate portable simulator output')
args = p.parse_args()
if any(name not in PROJECTS for name in args.projects):
    p.error('unknown project; choose: ' + ', '.join(PROJECTS))
sdk = Path(os.environ.get('ASTER_SDK', ROOT.parent / 'aster-sdk'))
for name in args.projects or PROJECTS:
    project = ROOT / name
    metadata = tomllib.loads((project / 'app.toml').read_text(encoding='utf-8-sig'))['app']
    workspace = project / 'build/sim' if args.no_aot else project
    env = os.environ | {'ASTER_WORKSPACE': str(workspace)}
    if name == 'GoldMiner':
        cmd = [sys.executable, str(project / 'tools/build.py'), '--target', args.target]
        if args.no_aot:
            cmd.append('--no-aot')
    elif name == 'OpenTyrian':
        cmd = [sys.executable, str(project / 'tools/build.py'), '--target', args.target]
        if not args.no_aot:
            cmd.append('--aot')
    else:
        cmd = [sys.executable, str(sdk / 'tools/build.py'), str(project), '--target', args.target,
               '--no-aot' if args.no_aot else '--aot']
    subprocess.run(cmd, env=env, check=True)
    if name == 'GoldMiner':
        print(f'Built {name}: {workspace}/dist/{args.target}; resources: {project}/dist/{args.target}/resources',flush=True)
        continue
    if name == 'OutRun':
        subprocess.run([sys.executable,str(project/'tools/prepare_store.py')],check=True)
        print(f'Built {name}: {workspace}/dist/{args.target}; resources: {project}/dist/store-resources',flush=True)
        continue
    resources = project / 'dist/resources'
    resources.mkdir(parents=True, exist_ok=True)
    if name != 'TouchTest':
        source = project / 'sdcard/aster/apps' / metadata['id'] / 'data'
        # Saves/configuration are not part of a release.
        for rel in ['resources', 'audio']:
            if (source / rel).is_dir():
                shutil.copytree(source / rel, resources / rel, dirs_exist_ok=True)
        if not (resources / 'resources').is_dir():
            raise RuntimeError(f'{name}: prepare external resources before publishing')
    if name == 'PixelWheels' and args.target == 'xtensa':
        subprocess.run([sys.executable, str(project/'tools/prepare_lckfb_assets.py')], check=True)
    print(f'Built {name}: {workspace}/dist/{args.target}; resources: {resources}', flush=True)
