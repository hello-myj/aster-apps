#!/usr/bin/env python3
"""Stage the S3 audio profile without modifying P4's full-rate resources."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import wave
import array
import sys
import os

ROOT = Path(__file__).resolve().parents[1]


def transcode(ffmpeg, source, dest, options):
    exe = ffmpeg or os.environ.get('FFMPEG') or shutil.which('ffmpeg')
    command = [exe] if exe else None
    if command is None and os.name == 'nt' and shutil.which('wsl'):
        command = ['wsl', '-d', 'Ubuntu-22.04', '--exec', 'ffmpeg']
        def linux(p):
            p = str(p.resolve()).replace('\\', '/')
            return '/mnt/' + p[0].lower() + p[2:]
        source, dest = linux(source), linux(dest)
    if command is None:
        raise SystemExit('Install ffmpeg or set FFMPEG before preparing lckfb audio')
    subprocess.run(command + ['-hide_banner', '-loglevel', 'error', '-y',
                              '-i', str(source)] + options + [str(dest)], check=True)


def prepare(ffmpeg):
    base = ROOT / 'dist/resources/resources'
    out = ROOT / 'dist/lckfb-resources/resources'
    out.mkdir(parents=True, exist_ok=True)
    records = []
    for source in sorted(base.iterdir()):
        if not source.is_file():
            continue
        dest = out / source.name
        if source.suffix == '.mp3':
            # A lower session rate also lowers the per-sample multi-voice mixer
            # work, rather than merely reducing the compressed download size.
            transcode(ffmpeg, source, dest,
                      ['-vn', '-map_metadata', '-1', '-ac', '1', '-ar', '16000',
                       '-c:a', 'libmp3lame', '-b:a', '64k'])
        elif source.name.startswith('engine') and source.suffix == '.wav':
            band = int(source.stem.removeprefix('engine')) + 1
            original = ROOT / f'vendor/upstream/android/assets/sounds/loop_{band}_0.wav'
            # Start with the original 44.1 kHz recording. The old converter
            # decimated by linear sampling without an anti-alias low-pass.
            with tempfile.TemporaryDirectory() as tmp:
                wav = Path(tmp) / 'filtered.wav'
                transcode(ffmpeg, original, wav,
                          ['-af', 'highpass=f=35,lowpass=f=3200,aresample=16000',
                           '-ac', '1', '-c:a', 'pcm_s16le'])
                with wave.open(str(wav), 'rb') as w:
                    params = w.getparams()
                    samples = array.array('h', w.readframes(w.getnframes()))
                if sys.byteorder != 'little':
                    samples.byteswap()
                peak = max(1, max(abs(v) for v in samples))
                # Leave more headroom for two engine bands plus music/cues.
                pcm = array.array('h', (round(v * 16000 / peak) for v in samples))
                if sys.byteorder != 'little':
                    pcm.byteswap()
                with wave.open(str(dest), 'wb') as w:
                    w.setparams(params)
                    w.writeframes(pcm.tobytes())
        else:
            shutil.copyfile(source, dest)
        records.append({'file': source.name, 'bytes': dest.stat().st_size})
    print(json.dumps({'profile': 'lckfb-16k-mono-filtered-engine',
                      'files': len(records), 'bytes': sum(r['bytes'] for r in records)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg')
    prepare(parser.parse_args().ffmpeg)
