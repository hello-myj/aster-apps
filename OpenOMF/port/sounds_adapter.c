/* Original PCM stays on SD. Upstream consumes a borrowed sample synchronously
 * through this port's WAV/effect backend before requesting another sample. */
#include "resources/sounds_loader.h"
#include "resources/resource_files.h"
#include "formats/sounds.h"
#include "utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {long offset;unsigned short length;unsigned char key;} Sample;
static Sample samples[SD_SOUNDS_MAX];
static path filename;
static char *buffer;
static unsigned capacity;
static int ready,last_id=-1;
static unsigned u16(const unsigned char *p){return p[0]|(p[1]<<8);}
static unsigned u32(const unsigned char *p){return u16(p)|(u16(p+2)<<16);}
void sounds_loader_close(void){free(buffer);buffer=NULL;capacity=0;ready=0;last_id=-1;memset(samples,0,sizeof(samples));}
bool sounds_loader_init(void){
 sounds_loader_close();filename=get_resource_filename("SOUNDS.DAT");
 FILE *f=fopen(path_c(&filename),"rb");if(!f)return false;
 unsigned char head[8];if(fread(head,1,8,f)!=8||u32(head)!=0)goto fail;
 unsigned header=u32(head+4);if(header<8||header%4||header/4-1>SD_SOUNDS_MAX)goto fail;
 if(fseek(f,0,SEEK_END))goto fail;long size=ftell(f);if(size<0||header>(unsigned)size||fseek(f,header,SEEK_SET))goto fail;
 unsigned count=header/4-1;
 for(unsigned i=0;i<count;i++){
  unsigned char h[3];if(fread(h,1,2,f)!=2)goto fail;
  unsigned n=u16(h);if(!n)continue;
  if(fread(h+2,1,1,f)!=1)goto fail;
  long off=ftell(f);if(off<0||off>size||(long)n>size-off)goto fail;
  samples[i]=(Sample){off,n,h[2]};if(n>capacity)capacity=n;
  if(fseek(f,n,SEEK_CUR))goto fail;
 }
 if(!capacity)goto fail;
 buffer=malloc(capacity);if(!buffer)goto fail;
 fclose(f);ready=1;
 log_info("Indexed original SOUNDS.DAT: %u samples, %u-byte raw buffer",count,capacity);
 return true;
 fail:fclose(f);sounds_loader_close();log_error("Unable to index original SOUNDS.DAT");return false;
}
bool sounds_loader_get(int id,char **out,int *length,int *freq){
 if(!ready||id<0||id>=SD_SOUNDS_MAX||!samples[id].length)return false;
 Sample *s=&samples[id];
 if(last_id!=id){
  FILE *f=fopen(path_c(&filename),"rb");if(!f)return false;
  int ok=!fseek(f,s->offset,SEEK_SET)&&fread(buffer,1,s->length,f)==s->length;
  fclose(f);if(!ok){last_id=-1;return false;}last_id=id;
 }
 *out=buffer;*length=s->length;*freq=1000000/(256-s->key);return true;
}
