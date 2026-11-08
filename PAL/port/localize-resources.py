"""Derive simplified TEXT and a covered 12px SC font; preserve all game chunks."""
import hashlib, json, struct, sys, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ENGINE = HERE.parent / 'sdlpal-aster'
sys.path[:0] = [str(HERE / 'python-deps'), str(ENGINE / 'tools')]
from opencc import OpenCC
import pal_pack_build as pack
from pal_ui_layout.font import parse_bdf

FONT_NAME = 'fusion-pixel-font-12px-monospaced-bdf-v2026.07.20.zip'
FONT_SHA = 'aea98326638e138de8583f0ae87db9eb722b9f44519361a32e0ee9577b3c6586'
convert = OpenCC('t2s').convert

def text_chunk(raw):
    fields = struct.unpack_from('<IHHHHIIIII', raw)
    words, messages, wt, mt, text_offset = fields[3:8]
    groups = []
    for count, table in [(words, wt), (messages, mt)]:
        offsets = struct.unpack_from('<' + 'I' * (count + 1), raw, table)
        groups.append([convert(raw[text_offset+a:text_offset+b].decode('utf-16le'))
                       for a, b in zip(offsets, offsets[1:])])
    payload, tables = bytearray(), []
    for group in groups:
        offsets = []
        for value in group:
            offsets.append(len(payload)); payload += value.encode('utf-16le')
        offsets.append(len(payload)); tables.append(struct.pack('<'+'I'*len(offsets), *offsets))
    start = pack.TEXT_HEADER_SIZE + len(tables[0]) + len(tables[1])
    header = struct.pack('<IHHHHIIIII', pack.TEXT_MAGIC, 1, pack.TEXT_HEADER_SIZE,
                         words, messages, pack.TEXT_HEADER_SIZE,
                         pack.TEXT_HEADER_SIZE+len(tables[0]), start, len(payload), 0)
    return header + b''.join(tables) + payload, sum(groups, [])

def font_chunk(texts, original_font):
    archive = HERE / 'font-sources' / FONT_NAME
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == FONT_SHA, 'Font archive hash mismatch'
    with zipfile.ZipFile(archive) as z:
        font = parse_bdf(z.read('fusion-pixel-12px-monospaced-zh_hans.bdf'))
    cps = sorted({ord(c) for t in texts for c in t if ord(c) >= 32} | set(range(32, 127)))
    missing = [cp for cp in cps if cp not in font.glyphs]
    count=struct.unpack_from('<H',original_font,8)[0]
    table,data=struct.unpack_from('<II',original_font,12)
    fallback={}
    for i in range(count):
        cp=struct.unpack_from('<H',original_font,table+i*2)[0]; simplified=convert(chr(cp))
        if len(simplified)==1:
            fallback[ord(simplified)]=original_font[data+i*32:data+(i+1)*32]
    unavailable=[cp for cp in missing if cp not in fallback]
    if unavailable: raise ValueError('Missing glyphs: ' + ','.join(hex(cp) for cp in unavailable))
    glyphs = bytearray()
    for cp in cps:
        bitmap = bytearray(32)
        if cp in missing:
            legacy=fallback[cp]
            for y in range(12):
                for x in range(12):
                    sx,sy=x*16//12,y*16//12
                    if legacy[sy*2+sx//8] & (0x80>>(sx%8)):bitmap[y*2+x//8]|=0x80>>(x%8)
            glyphs+=bitmap;continue
        glyph = font.glyphs[cp]
        left = glyph.bbox.x_offset - font.bbox.x_offset
        top = font.bbox.y_offset + font.bbox.height - glyph.bbox.y_offset - glyph.bbox.height
        for y in range(glyph.bbox.height):
            for x in range(glyph.bbox.width):
                if glyph.pixel(x,y):
                    dx,dy=x+left,y+top
                    if not (0<=dx<12 and 0<=dy<12): raise ValueError('Glyph outside 12px cell: '+hex(cp))
                    bitmap[dy*2+dx//8] |= 0x80 >> (dx%8)
        glyphs += bitmap
    table = struct.pack('<'+'H'*len(cps), *cps)
    offset = pack.align4(pack.FONT_HEADER_SIZE + len(table))
    header = struct.pack('<IHHHHIIIII', pack.FONT_MAGIC,1,pack.FONT_HEADER_SIZE,
                         len(cps),32,pack.FONT_HEADER_SIZE,offset,len(glyphs),len(cps),0)
    return header+table+bytes(offset-len(header)-len(table))+glyphs, len(cps), missing

def main():
    source = HERE / 'resources/pal_full.source.pak'
    target = HERE / 'resources/pal_full.pak'
    if not source.exists(): source.write_bytes(target.read_bytes())
    raw = source.read_bytes(); pack.verify_pack(raw)
    names = {v:k for k,v in pack.ARCHIVE_IDS.items()}; archives={}
    count=struct.unpack_from('<H',raw,8)[0]; at=struct.unpack_from('<I',raw,12)[0]
    for i in range(count):
        aid,n,table,_=struct.unpack_from('<HHII',raw,at+i*12); chunks=[]
        for j in range(n):
            offset,size,fmt,_,_=struct.unpack_from('<IIHHI',raw,table+j*16)
            chunks.append(pack.Chunk(raw[offset:offset+size],fmt))
        archives[names[aid]]=chunks
    localized, texts = text_chunk(archives['TEXT'][0].payload)
    # Include literal UI labels used outside the resource text tables.
    texts += ['无敌必胜金钱状态物品法术保存读取进度音乐音效系统是取消确认返回作弊',
              '無敵必勝金錢狀態物品法術保存讀取進度音樂音效系統取消確認返回作弊']
    font, glyph_count, fallback=font_chunk(texts,archives['FONT'][0].payload)
    archives['TEXT'][0]=pack.Chunk(localized,pack.FORMAT_TEXT_UTF16)
    archives['FONT'][0]=pack.Chunk(font,pack.FORMAT_FONT_GLYPHS)
    result=pack.build_pack(archives,pack.compute_portable_pack_set_id(archives));pack.verify_pack(result)
    target.write_bytes(result)
    report={'language':'zh-Hans','font':'Fusion Pixel 12px SC','glyphs':glyph_count,
            'textEntries':len(texts)-2,'fontArchiveSha256':FONT_SHA,'fallbackGlyphs':[chr(cp) for cp in fallback],
            'sourceSha256':hashlib.sha256(raw).hexdigest(),'outputSha256':hashlib.sha256(result).hexdigest(),
            'bytes':len(result),'sample':convert('讀取進度：李大娘，靈兒，寶劍，發現。')}
    (HERE/'resources/localization.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))

if __name__=='__main__': main()
