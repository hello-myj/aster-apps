"""Guard the native heap budget which the JavaScript simulator does not enforce."""
from pathlib import Path
import struct
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[1]

class MemoryBudget(unittest.TestCase):
    def test_runtime_heap_is_not_squeezed_by_initial_memory(self):
        app = tomllib.loads((ROOT / 'app.toml').read_text(encoding='utf-8-sig'))['app']
        # P4 reserves the requested host heap from max - initial, capped at 2 MiB.
        self.assertEqual(min(app['heap'], 2 * 1024**2, app['max_memory'] - app['initial_memory']), 2 * 1024**2)

    def test_all_course_transitions_fit_with_cache_released(self):
        resources = ROOT / 'sdcard/aster/apps/aster.outrun/data/resources'
        maps = []
        for p in resources.glob('map*.bin'):
            _, lines, metas, objects, *_ = struct.unpack('<10I', p.read_bytes()[:40])
            maps.append(lines * 20 + metas * 52 + objects * 20 + (lines + 1) * 4)
        self.assertEqual(len(maps), 16)
        font = (resources / 'font.r16').stat().st_size
        # Largest old and new course, native 480-square surface, font, allocator slack.
        self.assertLess(2 * max(maps) + 480 * 480 * 2 + font + 131072, 2 * 1024**2)

if __name__ == '__main__':
    unittest.main()
