#include "pal_pack.h"
#include "pal_level2_resident_pack.h"
#include <stdint.h>
/* Host-only capacity checker using the actual upstream native-pack builder. */
static uint8_t toc_owner[40960];
static uint8_t resident_owner[2097152];
static uint32_t resulting_size;
__attribute__((import_module("env"),import_name("pal_resource_read")))
extern int host_read(uint32_t offset,uint8_t *dst,uint32_t size);
static bool read_at(void *user,uint32_t offset,uint8_t *dst,uint32_t size)
{(void)user;return host_read(offset,dst,size)!=0;}
uint32_t audit_resident(uint32_t size,uint32_t mask,uint32_t capacity)
{
    PalPackToc toc;resulting_size=0;
    if(capacity>sizeof(resident_owner)||!PalPack_OpenTocRead(&toc,read_at,0,size,toc_owner,sizeof(toc_owner)))return 0;
    if(!PalLevel2ResidentPack_Build(&toc,mask,read_at,0,resident_owner,capacity,&resulting_size))return 0;
    return resulting_size;
}
