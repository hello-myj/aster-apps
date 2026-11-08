"""Check every nonempty source audio slot has its complete staged payload."""
from pathlib import Path
import re
import struct
import wave

root = Path(__file__).resolve().parents[1]
pack = (root/'port/resources/pal_full.pak').read_bytes()
catalog = (root/'port/include/audio_catalog.h').read_text()
audio = root/'dist/resources/audio'

def archive(wanted):
    count = struct.unpack_from('<H', pack, 8)[0]
    table = struct.unpack_from('<I', pack, 12)[0]
    for i in range(count):
        aid, n, offset, _ = struct.unpack_from('<HHII', pack, table+i*12)
        if aid == wanted:
            result = []
            for j in range(n):
                start, size, _, _, _ = struct.unpack_from('<IIHHI', pack, offset+j*16)
                assert start+size <= len(pack)
                result.append(pack[start:start+size])
            return result
    raise AssertionError(f'Missing audio archive {wanted}')

def values(name):
    return list(map(int, re.search(name+r'\[[^]]+\]=\{([^}]+)\}', catalog)[1].split(',')))

music, effects = archive(11), archive(19)
parts, samples = values('pal_audio_sfx_parts'), values('pal_audio_sfx_samples')
assert len(parts) == len(samples) == len(effects)
expected_music = {f'{i:03}.mp3' for i, song in enumerate(music) if song}
assert {p.name for p in audio.glob('*.mp3')} == expected_music
assert all((audio/name).stat().st_size > 100 for name in expected_music)
expected_clips = set()
for i, source in enumerate(effects):
    assert samples[i] == len(source)
    assert parts[i] == (len(source)+16299)//16300
    for part in range(parts[i]):
        name = f'{i:03}_{part}.wav'
        expected_clips.add(name)
        raw = source[part*16300:(part+1)*16300]
        with wave.open(str(audio/'sfx'/name), 'rb') as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) == (2, 2, 8192, len(raw))
            expected = b''.join(struct.pack('<hh', v*256, v*256) for v in struct.unpack(f'{len(raw)}b', raw))
            assert wav.readframes(len(raw)) == expected, name
assert {p.name for p in (audio/'sfx').glob('*.wav')} == expected_clips
print(f'PASS {len(expected_music)} music files and {len(expected_clips)} PCM clips covering {sum(bool(e) for e in effects)} nonempty effect slots')
