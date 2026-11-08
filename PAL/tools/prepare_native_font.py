"""Subset the pinned Aster 18px face onto PAL's native 18x18 UI cells."""
from pathlib import Path
import hashlib
import struct

ROOT=Path(__file__).resolve().parents[1]

def main():
    source=ROOT/'port/font-sources/aster-ui-18.cpfont'
    raw=source.read_bytes()
    assert hashlib.sha256(raw).hexdigest()=='fef8631bd559c5b7728966d0da6e531ea10043793bd463cc32aa06017f1317f4'
    # The source is vendored with its font notices; no host/system font lookup.
    assert raw[:8]==b'CPFONT\0\0' and struct.unpack_from('<HH',raw,8)==(4,0)
    toc=struct.unpack_from('<B3xIIBhhHHBBBI4x',raw,32)
    intervals,n,offset=toc[1],toc[2],toc[-1]
    ranges=[struct.unpack_from('<III',raw,offset+i*12) for i in range(intervals)]
    records=offset+intervals*12;bitmaps=records+n*16
    pack=(ROOT/'port/resources/pal_full.pak').read_bytes()
    for i in range(struct.unpack_from('<H',pack,8)[0]):
        aid,_,table,_=struct.unpack_from('<HHII',pack,struct.unpack_from('<I',pack,12)[0]+12*i)
        if aid==18: # FONT
            start,size,*_=struct.unpack_from('<IIHHI',pack,table)
            font=pack[start:start+size];break
    else: raise ValueError('Missing game font')
    count=struct.unpack_from('<H',font,8)[0];table=struct.unpack_from('<I',font,12)[0]
    cps=list(struct.unpack_from('<'+'H'*count,font,table))
    output=bytearray(b'PALUI18\0'+struct.pack('<HHI',1,count,162))
    output+=struct.pack('<'+'H'*count,*cps)
    for cp in cps:
        if cp==0x2500:
            cell=[15 if i//18==9 else 0 for i in range(324)]
            output+=bytes((cell[i]<<4)|cell[i+1] for i in range(0,324,2))
            continue
        lookup={0x2027:0x00B7}.get(cp,cp)
        index=next((base+lookup-first for first,last,base in ranges if first<=lookup<=last),None)
        if index is None: raise ValueError(f'Native UI font missing U+{cp:04X}')
        w,h,advance,left,top,length,pos=struct.unpack_from('<BBHhhH2xI',raw,records+index*16)
        data=raw[bitmaps+pos:bitmaps+pos+length]
        levels=[((data[i//2]>>(0 if i&1 else 4))&15) for i in range(w*h)]
        cell=[0]*324
        # PAL's fixed ASCII advance is half a CJK cell. Area-sample wide Latin
        # ink to that nine-pixel cell; Chinese remains on the native 18px grid.
        target=min(w,9 if cp<128 else 18)
        for y in range(h):
            dy=15-top+y
            if not 0<=dy<18: continue
            for x in range(target):
                dx=(max(0,left) if target==w else 0)+x
                if dx>=18: continue
                total=0
                for sx in range(w):
                    overlap=max(0,min((x+1)*w,(sx+1)*target)-max(x*w,sx*target))
                    total+=levels[y*w+sx]*overlap
                cell[dy*18+dx]=min(15,(total+w//2)//w)
        output+=bytes((cell[i]<<4)|cell[i+1] for i in range(0,324,2))
    out=ROOT/'dist/resources/resources/ui18.bin';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(output)
    print(f'Native UI font: {count} glyphs, {len(output)} bytes; source SHA256={hashlib.sha256(raw).hexdigest()}')

if __name__=='__main__':main()
