/* GPL-3.0-or-later. External SD data and bounded RGB565 image cache. */
#include "engine.h"
Map map;Image font;uint32_t cache_bytes,cache_peak,asset_errors;
static Image cache[80];static uint16_t cached_slot[2048];static uint32_t age;
/* OPK1: seekable SD archive. Header/index only live in RAM; pixels remain LRU. */
static bool bundle_checked,bundle_available;
static uint32_t image_entries[2048][2],map_entries[16][2],bundle_images,bundle_total,bundle_version,font_entry[2];
static const char bundle_path[]="/data/resources/resources.pak";
static int exact_read(aster_handle_t f,void *dst,uint32_t n){
 uint32_t at=0;while(at<n){int r=aster_fs_read(f,(uint8_t*)dst+at,n-at);if(r<=0)return r<0?r:ASTER_EIO;at+=r;}return 0;
}
static void bundle_probe(void){
 if(bundle_checked)return;bundle_checked=true;aster_handle_t f;
 int r=aster_fs_open(bundle_path,strlen(bundle_path),ASTER_FS_O_RDONLY,&f);if(r<0)return;
 uint32_t h[8];r=exact_read(f,h,32);
 if(r==0&&h[0]==0x314b504f&&(h[1]==1||h[1]==2)&&h[2]>0&&h[2]<=2048&&h[3]==16){
  bundle_version=h[1];bundle_images=h[2];font_entry[0]=h[4];font_entry[1]=h[5];bundle_total=h[6];
  r=exact_read(f,image_entries,bundle_images*8);if(r==0)r=exact_read(f,map_entries,16*8);
  if(r==0&&(bundle_version==2||(font_entry[0]<=bundle_total&&font_entry[1]<=bundle_total-font_entry[0])))bundle_available=true;
 }
 aster_fs_close(f);aster_logf(bundle_available?ASTER_LOG_INFO:ASTER_LOG_ERROR,"SD archive detected valid=%u images=%u",bundle_available,bundle_images);
}
static uint32_t decimal(const char *p){uint32_t n=0;while(*p>='0'&&*p<='9'){n=n*10+*p-'0';p++;}return n;}
static int resource_open(const char *rel,aster_handle_t *f,uint32_t *length){
 bundle_probe();
 if(bundle_available){
  uint32_t *e=0;
  if(!strcmp(rel,"font.r16"))e=font_entry;
  else if(!strncmp(rel,"img/",4)){uint32_t i=decimal(rel+4);if(i<bundle_images)e=image_entries[i];}
  else if(!strncmp(rel,"map",3)){uint32_t i=decimal(rel+3);if(i<16)e=map_entries[i];}
  if(!e)return ASTER_EIO;
  uint32_t offset=e[0];char chunk_path[80];const char *path=bundle_path;
  if(bundle_version==2){uint32_t chunk=offset>>24;offset&=0xffffff;if(chunk>31||offset>4194304||e[1]>4194304-offset)return ASTER_EIO;if(chunk){aster_snprintf(chunk_path,sizeof(chunk_path),"/data/resources/resources%02u.pak",chunk);path=chunk_path;}}
  else if(offset>bundle_total||e[1]>bundle_total-offset)return ASTER_EIO;
  int r=aster_fs_open(path,strlen(path),ASTER_FS_O_RDONLY,f);if(r<0)return r;
  r=aster_fs_seek(*f,(int32_t)offset,ASTER_FS_SEEK_SET);if(r<0){aster_fs_close(*f);return r;}
  if(length)*length=e[1];return 0;
 }
 char path[160];aster_snprintf(path,sizeof(path),"/data/resources/%s",rel);if(length)*length=0xffffffffu;
 return aster_fs_open(path,strlen(path),ASTER_FS_O_RDONLY,f);
}int read_file(const char *rel,void *dst,uint32_t size){
 aster_handle_t f;uint32_t available;int r=resource_open(rel,&f,&available);if(r<0)return r;
 if(size>available){aster_fs_close(f);return ASTER_EIO;} uint32_t at=0;while(at<size){r=aster_fs_read(f,(uint8_t*)dst+at,size-at);if(r<=0){aster_fs_close(f);return ASTER_EIO;}at+=r;}
 return aster_fs_close(f);
}
void free_map(void){free(map.line);free(map.meta);free(map.obj);free(map.first);memset(&map,0,sizeof(map));}
int load_map(uint32_t id){
 char path[128];aster_snprintf(path,sizeof(path),"map%02u.bin",id);
 aster_handle_t f;uint32_t available;int r=resource_open(path,&f,&available);if(r<0)return r;
 Map next={0};uint8_t *parts[5];uint32_t sizes[5];r=aster_fs_read(f,&next.h,sizeof(next.h));
 if(r!=sizeof(next.h)||next.h.magic!=0x3252544f||next.h.count>20000||next.h.metas>512||next.h.objects>100000){aster_fs_close(f);return ASTER_EIO;}
 next.line=malloc(next.h.count*sizeof(RoadLine));next.meta=malloc(next.h.metas*sizeof(ObjectMeta));next.obj=malloc(next.h.objects*sizeof(Placement));next.first=calloc(next.h.count+1,4);
 if(!next.line||!next.meta||!next.obj||!next.first){free(next.line);free(next.meta);free(next.obj);free(next.first);aster_fs_close(f);return ASTER_ENOMEM;}
 parts[0]=(uint8_t*)next.colors;parts[1]=(uint8_t*)next.line;parts[2]=(uint8_t*)next.meta;parts[3]=(uint8_t*)next.obj;
 sizes[0]=24;sizes[1]=next.h.count*sizeof(RoadLine);sizes[2]=next.h.metas*sizeof(ObjectMeta);sizes[3]=next.h.objects*sizeof(Placement);
 for(int p=0;p<4;p++){uint32_t done=0;while(done<sizes[p]){r=aster_fs_read(f,parts[p]+done,sizes[p]-done);if(r<=0)goto bad;done+=r;}}
 aster_fs_close(f);
 for(uint32_t i=0;i<next.h.objects;i++){if(next.obj[i].line>=next.h.count||next.obj[i].object>=next.h.metas){r=ASTER_EIO;goto release;}next.first[next.obj[i].line+1]++;}
 for(uint32_t i=1;i<=next.h.count;i++)next.first[i]+=next.first[i-1];
 free_map();map=next;aster_logf(ASTER_LOG_INFO,"MAP %u lines=%u objects=%u exact relief loaded",id,map.h.count,map.h.objects);return 0;
bad:aster_fs_close(f);
release:free(next.line);free(next.meta);free(next.obj);free(next.first);return r<0?r:ASTER_EIO;
}
static void evict(int slot){if(cache[slot].pixel){if(cache[slot].id<2048)cached_slot[cache[slot].id]=0;cache_bytes-=cache[slot].bytes;free(cache[slot].pixel);}memset(&cache[slot],0,sizeof(Image));}
static int oldest(void){int o=0;for(int i=0;i<80;i++){if(!cache[i].pixel)return i;if(cache[i].last<cache[o].last)o=i;}return o;}
Image *get_image(uint32_t id){
 age++;if(id>=2048){asset_errors++;return 0;}int found=(int)cached_slot[id]-1;
 if(found>=0){cache[found].last=age;profile.hits++;return &cache[found];}
 uint32_t io_start=aster_time_now_ms();profile.misses++;
 int slot=oldest();evict(slot);char name[40];aster_snprintf(name,sizeof(name),"img/%04u.r16",id);
 aster_handle_t f;uint32_t available;
 if(resource_open(name,&f,&available)<0){asset_errors++;return 0;}
 uint16_t header[4];if(aster_fs_read(f,header,8)!=8){aster_fs_close(f);asset_errors++;return 0;}bool alpha=header[0]&0x8000;header[0]&=0x7fff;if(!header[0]||!header[1]||header[0]>2048||header[1]>1024){aster_fs_close(f);asset_errors++;return 0;}
 uint32_t bytes=header[0]*header[1]*(alpha?3:2);if(bytes>1350000||bytes+8>available){aster_fs_close(f);asset_errors++;return 0;}
 uint32_t allocated=((bytes+3)&~3u)+header[1]*4;
 while(cache_bytes+allocated>1350000){int o=-1;for(int i=0;i<80;i++)if(cache[i].pixel&&(o<0||cache[i].last<cache[o].last))o=i;if(o<0)break;evict(o);}
 uint16_t *data=malloc(allocated);if(!data){aster_fs_close(f);asset_errors++;return 0;}
 uint32_t done=0;while(done<bytes){int r=aster_fs_read(f,(uint8_t*)data+done,bytes-done);if(r<=0){free(data);aster_fs_close(f);asset_errors++;return 0;}done+=r;}aster_fs_close(f);
 uint8_t *alpha_plane=alpha?(uint8_t*)(data+header[0]*header[1]):0;
 uint16_t *left=(uint16_t*)((uint8_t*)data+((bytes+3)&~3u)),*right=left+header[1];
 for(uint32_t y=0;y<header[1];y++){uint32_t l=0,r=header[0],at=y*header[0];
  while(l<r&&(!data[at+l]||(alpha_plane&&!alpha_plane[at+l])))l++;
  while(r>l&&(!data[at+r-1]||(alpha_plane&&!alpha_plane[at+r-1])))r--;
  left[y]=l;right[y]=r;
 }
 cached_slot[id]=slot+1;cache[slot]=(Image){header[0],header[1],header[2],header[3],data,id,age,allocated,alpha_plane,left,right};cache_bytes+=allocated;cache_peak=MAX(cache_peak,cache_bytes);profile.io+=(uint32_t)(aster_time_now_ms()-io_start);profile.bytes+=bytes+8;return &cache[slot];
}
int load_font(void){uint16_t h[4];int r=read_file("font.r16",h,8);if(r<0)return r;if(h[0]!=256||h[1]!=96)return ASTER_EIO;uint32_t n=8+h[0]*h[1]*2;uint8_t *b=malloc(n);if(!b)return ASTER_ENOMEM;r=read_file("font.r16",b,n);if(r<0){free(b);return r;}font=(Image){h[0],h[1],h[2],h[3],(uint16_t*)(b+8),0,0,n,0,0,0};return 0;}
void free_images(void){for(int i=0;i<80;i++)evict(i);if(font.pixel)free((uint8_t*)font.pixel-8);memset(&font,0,sizeof(font));}





