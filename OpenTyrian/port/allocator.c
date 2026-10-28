/* Guest-owned C heap. BSS storage keeps it below __heap_base, separate from
 * Aster's Host-managed surface/object scratch allocations. No memory.grow. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#ifndef TYRIAN_HEAP_BYTES
#define TYRIAN_HEAP_BYTES (16u*1024u*1024u)
#endif
#define NONE 0xffffffffu
int tyrian_drop_cache(void);
typedef struct {uint32_t size,prev,next,free;} Block;
typedef struct {uint32_t prev,next;} Links;
static _Alignas(16) unsigned char pool[TYRIAN_HEAP_BYTES];
static int initialized;static uint32_t used,peak,free_head;
static Block *block(uint32_t off){return (Block*)(pool+off);}
static Links *links(uint32_t off){return (Links*)(pool+off+sizeof(Block));}
static void init(void){if(initialized)return;*block(0)=(Block){.size=TYRIAN_HEAP_BYTES-sizeof(Block),.prev=NONE,.next=NONE,.free=1};*links(0)=(Links){NONE,NONE};free_head=0;initialized=1;}
static void remove_free(uint32_t at){Links l=*links(at);if(l.prev==NONE)free_head=l.next;else links(l.prev)->next=l.next;if(l.next!=NONE)links(l.next)->prev=l.prev;}
static void insert_free(uint32_t at){uint32_t prev=NONE,next=free_head;while(next!=NONE&&next<at){prev=next;next=links(next)->next;}*links(at)=(Links){prev,next};if(prev==NONE)free_head=at;else links(prev)->next=at;if(next!=NONE)links(next)->prev=at;}
static void merge_physical(uint32_t at){Block *b=block(at),*n=block(b->next);b->size+=sizeof(Block)+n->size;b->next=n->next;if(n->next!=NONE)block(n->next)->prev=at;}
static void split(uint32_t at,uint32_t size){Block *b=block(at);if(b->size-size<sizeof(Block)+16)return;uint32_t next=at+sizeof(Block)+size;*block(next)=(Block){.size=b->size-size-sizeof(Block),.prev=at,.next=b->next,.free=1};if(b->next!=NONE)block(b->next)->prev=next;b->next=next;b->size=size;insert_free(next);}
static Block *checked(void *p){uintptr_t x=(uintptr_t)p,base=(uintptr_t)pool;if(x<base+sizeof(Block)||x>=base+TYRIAN_HEAP_BYTES||(x-base)%16)__builtin_trap();Block *b=(Block*)((unsigned char*)p-sizeof(Block));if(b->free||b->size>TYRIAN_HEAP_BYTES||x+b->size>base+TYRIAN_HEAP_BYTES)__builtin_trap();return b;}
/* Address-ordered first fit on free blocks only. Tiny parser allocations do
 * not rescan live objects; retaining the large tail reduces fragmentation. */
void *malloc(size_t n){init();if(!n)n=1;if(n>TYRIAN_HEAP_BYTES-32){errno=ENOMEM;return NULL;}uint32_t wanted=(n+15u)&~15u;retry:;for(uint32_t at=free_head;at!=NONE;at=links(at)->next){Block *b=block(at);if(b->size<wanted)continue;remove_free(at);split(at,wanted);b->free=0;used+=b->size;if(used>peak)peak=used;return (unsigned char*)b+sizeof(Block);}if(tyrian_drop_cache())goto retry;errno=ENOMEM;return NULL;}
void free(void *p){if(!p)return;Block *b=checked(p);used-=b->size;b->free=1;uint32_t at=(unsigned char*)b-pool;if(b->next!=NONE&&block(b->next)->free){remove_free(b->next);merge_physical(at);}if(b->prev!=NONE&&block(b->prev)->free){merge_physical(b->prev);}else insert_free(at);}
void *calloc(size_t n,size_t z){if(z&&n>((size_t)-1)/z){errno=ENOMEM;return NULL;}size_t bytes=n*z;void *p=malloc(bytes);if(p)memset(p,0,bytes);return p;}
void *realloc(void *p,size_t n){if(!p)return malloc(n);if(!n){free(p);return NULL;}Block *b=checked(p);if(n<=b->size)return p;uint32_t at=(unsigned char*)b-pool,old=b->size;if(b->next!=NONE&&block(b->next)->free&&n<=b->size+sizeof(Block)+block(b->next)->size){remove_free(b->next);merge_physical(at);split(at,(n+15u)&~15u);used+=b->size-old;if(used>peak)peak=used;return p;}void *q=malloc(n);if(q){memcpy(q,p,old);free(p);}return q;}
void *__libc_malloc(size_t n){return malloc(n);}void __libc_free(void *p){free(p);}void *__libc_calloc(size_t n,size_t s){return calloc(n,s);}void *__libc_realloc(void *p,size_t n){return realloc(p,n);}
size_t malloc_usable_size(void *p){return p?checked(p)->size:0;}
void *aligned_alloc(size_t alignment,size_t size){if(!alignment||(alignment&(alignment-1))||alignment>16||size%alignment){errno=EINVAL;return NULL;}return malloc(size);}
int posix_memalign(void **out,size_t alignment,size_t size){if(alignment<sizeof(void*)||(alignment&(alignment-1))||alignment>16)return EINVAL;void *p=malloc(size);if(!p)return ENOMEM;*out=p;return 0;}
uint32_t tyrian_heap_used(void){return used;}uint32_t tyrian_heap_peak(void){return peak;}uint32_t tyrian_heap_capacity(void){return TYRIAN_HEAP_BYTES;}
