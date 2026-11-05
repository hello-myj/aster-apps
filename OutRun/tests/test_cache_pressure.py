"""A fragmented native heap must evict a loaded image even with empty slots."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT.parent.parent / 'aster-sdk'

class CachePressure(unittest.TestCase):
    def test_allocation_retry_reclaims_loaded_slot(self):
        code = r'''
#include <assert.h>
#include "engine.h"
static unsigned attempts, released;
static uint16_t pixel_data[1024];
static void *limited_malloc(size_t bytes){assert(bytes<sizeof(pixel_data));if(++attempts==1)return 0;assert(released==1);return pixel_data;}
static void limited_free(void *p){assert(p==(void*)0x1234);released++;}
#define malloc limited_malloc
#define free limited_free
#include "assets.c"
#undef malloc
#undef free
uint32_t screen_w=480,screen_h=480;
FrameProfile profile;
uint32_t aster_time_now_ms(void){return 0;}
void aster_log(uint32_t level,const char *msg,uint32_t len){(void)level;(void)msg;(void)len;}
int32_t aster_fs_open(const char *p,uint32_t n,uint32_t flags,aster_handle_t *f){(void)p;(void)n;(void)flags;*f=1;return 0;}
int32_t aster_fs_read(aster_handle_t f,void *data,uint32_t n){(void)f;memset(data,0,n);if(n==8){uint16_t *h=data;h[0]=16;h[1]=16;h[2]=16;h[3]=16;}return n;}
int32_t aster_fs_close(aster_handle_t f){(void)f;return 0;}
int32_t aster_fs_seek(aster_handle_t f,int32_t off,uint32_t how){(void)f;(void)how;return off;}
int main(void){
 bundle_checked=true;
 cache[10]=(Image){.pixel=(void*)0x1234,.bytes=1000,.id=12,.last=1};
 cached_slot[12]=11;cache_bytes=1000;
 Image *im=get_image(1);
 assert(im&&im->pixel==pixel_data&&attempts==2&&released==1);
 assert(cached_slot[12]==0&&cached_slot[1]>0&&asset_errors==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            source = Path(d) / 'pressure.c'
            source.write_text(code)
            binary = Path(d) / 'pressure'
            includes = [ROOT/'src/port', SDK/'include', SDK/'lib/aster']
            subprocess.run(['cc', '-std=c11', '-fno-builtin', '-Wno-attributes',
                            *[f'-I{x}' for x in includes], str(source),
                            str(SDK/'lib/aster/aster_printf.c'), '-o', str(binary)],
                           check=True, text=True)
            subprocess.run([str(binary)], check=True, timeout=5)

if __name__ == '__main__':
    unittest.main()
