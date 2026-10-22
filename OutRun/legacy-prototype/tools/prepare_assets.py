#!/usr/bin/env python3
"""Convert upstream OutRun Map1 road profile and selected sprites deterministically."""
from pathlib import Path
import json
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]; UP=ROOT.parent/'vendor'/'upstream'/'Resources'; OUT=ROOT/'sdcard'/'aster'/'apps'/'aster.outrun'/'data'/'resources'; OUT.mkdir(parents=True,exist_ok=True)
sources={'player.png':UP/'Vehicles'/'PlayerCar'/'Ferraris'/'Ferrari1'/'c1.png','traffic.png':UP/'Vehicles'/'TrafficCars'/'Car1'/'c1.png','palm.png':UP/'Maps'/'MapLevels'/'Map1'/'3.png'}; manifest=[]
for name,src in sources.items():
 im=Image.open(src).convert('RGBA'); original=im.size; box=im.getchannel('A').getbbox()
 if box: im=im.crop(box)
 im.thumbnail((96,128),Image.Resampling.LANCZOS); im.save(OUT/name,optimize=True)
 manifest.append({'file':name,'source':str(src.relative_to(ROOT.parent)),'source_size':original,'output_size':im.size,'format':'PNG RGBA; transparent margins cropped','bytes':(OUT/name).stat().st_size})
for name,label,fill in [('control-steer.png','L/R',(36,74,98,255)),('control-brake.png','BRAKE',(130,42,43,255))]:
 im=Image.new('RGBA',(96,44),(0,0,0,0)); d=ImageDraw.Draw(im); d.rounded_rectangle((1,1,94,42),radius=10,fill=fill,outline=(199,226,238,255),width=2)
 if name=='control-steer.png':
  d.polygon([(18,22),(34,10),(34,34)],fill=(255,255,255,255)); d.polygon([(78,22),(62,10),(62,34)],fill=(255,255,255,255))
 else: d.text((24,15),label,fill=(255,255,255,255))
 im.save(OUT/name,optimize=True); manifest.append({'file':name,'source':'Aster touch overlay for this port','output_size':im.size,'format':'PNG RGBA black-key sprite; display alpha 150/255','bytes':(OUT/name).stat().st_size})
mapfile=UP/'Maps'/'MapLevels'/'Map1'/'map.txt'; txt=mapfile.read_text(encoding='utf-8'); rows=[]; active=False
for line in txt.splitlines():
 s=line.strip()
 if s=='START_RELIEF': active=True; continue
 if s=='SPRITES': break
 if not active or ':' not in s: continue
 typ,rest=s.split(':',1); vals=rest.split()
 if typ not in ('STRAIGHT','CURVE_LEFT','CURVE_RIGHT','HILL_STRAIGHT','HILL_LEFT','HILL_RIGHT'): continue
 try: nums=[float(x) for x in vals]
 except ValueError: continue
 if len(nums)<3: continue
 curve=0.0
 if typ in ('CURVE_LEFT','CURVE_RIGHT') and len(nums)>=4: curve=nums[3]
 elif typ in ('HILL_LEFT','HILL_RIGHT') and len(nums)>=5: curve=nums[4] # Source parser: slope at nums[3], curve direction at nums[4].
 rows.append({'type':typ,'enter':nums[0],'hold':nums[1],'leave':nums[2],'curve':curve})
active_rows=[r for r in rows if r['enter']+r['hold']+r['leave']>0]; total=sum(r['enter']+r['hold']+r['leave'] for r in active_rows)
if total<=0: raise SystemExit(f'No nonzero Map1 relief distance in {mapfile}')
samples=[]
for i in range(2048):
 pos=i/2048.0*total; row=active_rows[-1]
 for candidate in active_rows:
  weight=candidate['enter']+candidate['hold']+candidate['leave']
  if pos<weight: row=candidate; break
  pos-=weight
 e,h,l=row['enter'],row['hold'],row['leave']
 if e>0 and pos<e: factor=pos/e
 elif pos<e+h: factor=1.0
 elif l>0: factor=max(0.0,1.0-(pos-e-h)/l)
 else: factor=0.0
 samples.append(max(-48,min(48,round(row['curve']*factor*10))))
(OUT/'track.bin').write_bytes(bytes(v&255 for v in samples))
manifest.append({'file':'track.bin','source':str(mapfile.relative_to(ROOT.parent)),'format':'2048 signed curvature samples resampled from Map1 START_RELIEF','bytes':2048,'sections':len(rows),'zero_length_sections':sum(1 for r in rows if r['enter']+r['hold']+r['leave']==0),'source_rows':rows})
(OUT/'resources.json').write_text(json.dumps({'upstream':'ZgzInfinity/OutRun GPL-3.0','map':'Map1','resources':manifest},ensure_ascii=False,indent=2),encoding='utf-8')
embed=ROOT/'resources'; embed.mkdir(parents=True,exist_ok=True)
for file in OUT.iterdir():
 if file.is_file() and file.name!='resources.json': (embed/file.name).write_bytes(file.read_bytes())
print(f'Wrote {len(manifest)} resources to {OUT}; Map1 relief sections={len(rows)}, samples=2048; embedded fallback={embed}')
