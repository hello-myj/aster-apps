#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the free public channel and export immutable GitHub Release assets."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, default=ROOT.parent/'aster-sdk')
    p.add_argument('--firmware', type=Path, default=ROOT.parent/'aster-firmware')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repository', default='hello-myj/aster-releases')
    p.add_argument('--tag', required=True)
    p.add_argument('--app', action='append', default=[])
    p.add_argument('--skip-build', action='store_true')
    a = p.parse_args();a.sdk=a.sdk.resolve();a.firmware=a.firmware.resolve();a.output=a.output.resolve()
    if a.output.exists():p.error('Output exists; choose a new tag/output directory')
    work = a.output.parent/('input-'+a.tag)
    if work.exists():p.error('Staging input exists; choose a new tag/output directory')
    work.mkdir(parents=True)
    sys.path[:0] = [str(a.sdk/'tools'), str(ROOT)]
    import setup_server, static_distribution
    from StoreServer.server import publish, Store
    channel = f'https://github.com/{a.repository}/releases/latest/download/aster-source.json'
    profiles = [('riscv','osptek-p4-st7102',[480,480]),('xtensa','aster.esp32s3.lckfb',[240,320])]
    for target, board, screen in profiles:
        environment = dict(os.environ)
        if not a.skip_build:
            subprocess.run([sys.executable,str(ROOT/'AppStore/tools/build.py'),'--sdk',str(a.sdk),
                            '--target',target,'--server-url',channel],env=environment,check=True)
        setup_server.publish(work/'setup',a.firmware/'resources/fonts',ROOT/f'AppStore/dist/{target}/aster.store.ast',
                             board,target,'1.1.0',channel,[screen])
        for project in a.app or ['TouchTest']:
            app=(ROOT/project).resolve(strict=True)
            if not app.is_relative_to(ROOT):raise ValueError('APP project must be inside APPS')
            config=tomllib.loads((app/'store.toml').read_text(encoding='utf-8-sig'))
            variants=[v for v in config['variants'] if v['target']==target and board in v['boards']]
            if not variants:continue
            if len(variants)!=1:raise ValueError('Ambiguous board/target variant')
            v=variants[0]
            if not a.skip_build:
                subprocess.run([sys.executable,str(a.sdk/'tools/build.py'),str(app),'--target',target,'--aot'],
                               env=environment|{'ASTER_WORKSPACE':str(app)},check=True)
            package=(app/v['package']).resolve(strict=True)
            resources=(app/v['resources']).resolve(strict=True) if v.get('resources') else None
            if not package.is_relative_to(app) or (resources and not resources.is_relative_to(app)):
                raise ValueError('Variant paths must stay inside the APP project')
            publish(work/'store',package,[board],resources,v.get('screens'))
    store=Store(work/'store');icons=work/'store/icons';icons.mkdir(parents=True)
    for key in store.releases:
        data=store.icon(key)
        if data:(icons/(key+'.bmp')).write_bytes(data)
    def revision(root):
        return subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    notices='Aster public distribution\n\nCorresponding source revisions:\n'
    for name,root in [('aster-sdk',a.sdk),('aster-firmware',a.firmware),('aster-apps',ROOT)]:
        notices+=f'https://github.com/hello-myj/{name}/tree/{revision(root)}\n'
    for name,path in [('System font notices',a.firmware/'resources/fonts/NOTICE'),
                      ('DroidSansFallbackFull license',a.firmware/'resources/fonts/DroidSansFallbackFull.LICENSE'),
                      ('APP license',ROOT/'LICENSE')]:
        notices+='\n\n'+name+'\n'+path.read_text(encoding='utf8')
    print(static_distribution.export(work/'setup',work/'store',a.output,a.repository,a.tag,[b for _,b,_ in profiles],notices))


if __name__=='__main__':main()
