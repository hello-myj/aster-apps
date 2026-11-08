"""Check native font cells, baseline and half-width Latin coverage."""
from pathlib import Path
import struct

data=(Path(__file__).resolve().parents[1]/'dist/resources/resources/ui18.bin').read_bytes()
assert data[:8]==b'PALUI18\0'
version,count,stride=struct.unpack_from('<HHI',data,8)
assert version==1 and 0<count<=3000 and stride==162
assert len(data)==16+count*(2+stride)
cps=struct.unpack_from('<'+'H'*count,data,16)
assert list(cps)==sorted(set(cps))
def ink(cp):
    start=16+count*2+cps.index(cp)*stride
    raw=data[start:start+stride]
    return [(i%18,i//18) for i in range(324) if (raw[i//2]>>(0 if i&1 else 4))&15]
stroke=ink(ord('一'))
assert stroke and 6<=min(y for x,y in stroke)<=10 and max(y for x,y in stroke)<=11
for cp in range(33,127):
    pixels=ink(cp)
    # Preserve the face's small side bearing; the ink itself is <=9px wide.
    assert not pixels or max(x for x,y in pixels)-min(x for x,y in pixels)<9,chr(cp)
for cp in map(ord,'仙剑确认返回状态物品保存读取'):
    assert len(ink(cp))>15,hex(cp)
print(f'PASS {count} native font cells, centered horizontal stroke and nine-pixel ASCII ink')
