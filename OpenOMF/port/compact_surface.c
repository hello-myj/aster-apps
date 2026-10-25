/* Lossless sprite storage. Pixels are exact; drawing and collision share
 * the same bounded LRU. Mutable surfaces are promoted to ordinary ownership.
 * Only individually allocated sprite surfaces are packed (never moving fonts). */
#include "omf_surface.h"
#include "utils/log.h"
#include "utils/crash.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define CACHE_LIMIT (128u*1024u)
#define BUCKETS 256
_Static_assert(sizeof(vga_pixel)==1,"Compact storage requires vanilla VGA");
typedef struct Backing {
    surface *s;
    struct Backing *hash_next,*prev,*next;
    unsigned packed,unpacked,format;
    unsigned char *external;
    unsigned char bytes[];
} Backing;
static Backing *buckets[BUCKETS],*oldest,*newest;
static unsigned resident,saved;
static unsigned bucket(const surface *s){return ((uintptr_t)s>>4)%BUCKETS;}
static Backing *find(const surface *s){for(Backing *b=buckets[bucket(s)];b;b=b->hash_next)if(b->s==s)return b;return NULL;}
static void unlink_lru(Backing *b){
    if(b->prev)b->prev->next=b->next;else oldest=b->next;
    if(b->next)b->next->prev=b->prev;else newest=b->prev;
    b->prev=b->next=NULL;
}
static void touch(Backing *b){
    if(b==newest)return;
    if(b->prev||b->next||b==oldest)unlink_lru(b);
    b->prev=newest;if(newest)newest->next=b;else oldest=b;newest=b;
}
static void evict(void){
    Backing *b=oldest;if(!b)return;unlink_lru(b);
    free(b->s->data);b->s->data=NULL;resident-=b->unpacked;
}
/* PackBits: 1..128 literal pixels or repeated pixels per record. */
static unsigned encode(const unsigned char *src,unsigned n,unsigned char *dst){
    unsigned at=0,out=0;
    while(at<n){
        unsigned run=1;while(run<128&&at+run<n&&src[at+run]==src[at])run++;
        if(run>=3){if(dst){dst[out]=(unsigned char)(0x80|(run-1));dst[out+1]=src[at];}out+=2;at+=run;continue;}
        unsigned start=at;at+=run;
        while(at<n&&at-start<128){
            run=1;while(run<3&&at+run<n&&src[at+run]==src[at])run++;
            if(run>=3)break;
            unsigned take=run;if(take>128-(at-start))take=128-(at-start);at+=take;
        }
        unsigned len=at-start;if(dst){dst[out]=(unsigned char)(len-1);memcpy(dst+out+1,src+start,len);}out+=1+len;
    }
    return out;
}
/* Preserve the original sparse VGA stream and decode only visible/colliding
 * frames. Loading a fighter no longer expands and recompresses every pose. */
int omf_surface_encoded(surface *s,int w,int h,char **data,unsigned len) {
    unsigned n=(unsigned)w*h;
    if(w<=0||h<=0||!len||len+sizeof(Backing)+64>=n)return 0;
    Backing *b=malloc(sizeof(*b));if(!b)return 0;
    surface_create(s,1,1);free(s->data);s->data=NULL;
    s->w=s->render_w=w;s->h=s->render_h=h;
    *b=(Backing){.s=s,.packed=len,.unpacked=n,.format=1,.external=(unsigned char *)*data};*data=NULL;
    unsigned hash=bucket(s);b->hash_next=buckets[hash];buckets[hash]=b;
    saved+=n-len-sizeof(*b);return 1;
}
void omf_surface_pack(surface *s){
    if(!s||!s->data||s->w<=0||s->h<=0||find(s))return;
    unsigned n=(unsigned)s->w*s->h,packed=encode(s->data,n,NULL);
    if(packed+sizeof(Backing)+64>=n)return;
    Backing *b=malloc(sizeof(*b)+packed);if(!b)return;
    *b=(Backing){.s=s,.packed=packed,.unpacked=n};encode(s->data,n,b->bytes);
    unsigned h=bucket(s);b->hash_next=buckets[h];buckets[h]=b;
    saved+=n-packed-sizeof(*b);free(s->data);s->data=NULL;
}
const vga_pixel *omf_surface_pixels(const surface *source){
    if(!source)return NULL;
    surface *s=(surface *)source;Backing *b=find(s);
    if(!b)return s->data;
    if(s->data){touch(b);return s->data;}
    while(oldest&&resident+b->unpacked>CACHE_LIMIT)evict();
    unsigned char *dst=malloc(b->unpacked);
    while(!dst&&oldest){evict();dst=malloc(b->unpacked);}
    if(!dst)crash_with_args("Sprite decode OOM: %u bytes",b->unpacked);
    unsigned in=0,out=0;
    if(b->format==1) {
        const unsigned char *encoded=b->external;
        unsigned x=0,y=0;memset(dst,0,b->unpacked);
        while(in+1<b->packed) {
            unsigned word=encoded[in]|((unsigned)encoded[in+1]<<8),op=word&3,count=word>>2;in+=2;
            if(op==0)x=count;
            else if(op==2)y=count;
            else if(op==1) {
                unsigned pos=y*(unsigned)s->w+x,take=count;
                if(take>b->packed-in)take=b->packed-in;
                if(pos>=b->unpacked)break;
                if(take>b->unpacked-pos)take=b->unpacked-pos;
                memcpy(dst+pos,encoded+in,take);in+=take;x=0;
                if(take<count)break;
            }
        }
        s->data=dst;resident+=b->unpacked;touch(b);return s->data;
    }
    while(in<b->packed){
        unsigned op=b->bytes[in++],count=(op&127)+1;
        if(count>b->unpacked-out)crash("Corrupt sprite cache length");
        if(op&128){if(in>=b->packed)crash("Corrupt sprite cache run");memset(dst+out,b->bytes[in++],count);}
        else{if(count>b->packed-in)crash("Corrupt sprite cache literal");memcpy(dst+out,b->bytes+in,count);in+=count;}
        out+=count;
    }
    if(out!=b->unpacked)crash("Truncated sprite cache");
    s->data=dst;resident+=b->unpacked;touch(b);return s->data;
}
void omf_surface_forget(surface *s){
    if(!s)return;
    Backing **at=&buckets[bucket(s)];while(*at&&(*at)->s!=s)at=&(*at)->hash_next;
    Backing *b=*at;if(!b)return;*at=b->hash_next;
    if(s->data){unlink_lru(b);resident-=b->unpacked;}
    saved-=b->unpacked-b->packed-sizeof(*b);free(b->external);free(b);
}
void omf_surface_promote(surface *s){if(s){(void)omf_surface_pixels(s);omf_surface_forget(s);}}
unsigned omf_surface_saved(void){return saved;}
unsigned omf_surface_resident(void){return resident;}
