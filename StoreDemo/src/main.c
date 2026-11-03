/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
static aster_handle_t surface;
static aster_ge_t ge;static aster_gfx_cmd_t commands[16];static uint8_t arena[512];
ASTER_APP_EXPORT int aster_app_init(void)
{
    const char text[]="/assets/message.txt",blob[]="/assets/pattern.bin";
    const char expected[]="ASTER_STORE_RESOURCE_V1\n";char message[64];aster_handle_t f;
    int r=aster_fs_open(text,sizeof(text)-1,ASTER_FS_O_RDONLY,&f);if(r)return r;
    int n=aster_fs_read(f,message,sizeof(message));r=aster_fs_close(f);
    if(r || n!=sizeof(expected)-1 || memcmp(message,expected,sizeof(expected)-1))return ASTER_ECORRUPT;
    r=aster_fs_open(blob,sizeof(blob)-1,ASTER_FS_O_RDONLY,&f);if(r)return r;
    uint8_t bytes[256];n=aster_fs_read(f,bytes,sizeof(bytes));
    if(n!=sizeof(bytes)){aster_fs_close(f);return ASTER_ECORRUPT;}
    for(unsigned i=0;i<256;i++)if(bytes[i]!=(uint8_t)(i%251)){aster_fs_close(f);return ASTER_ECORRUPT;}
    r=aster_fs_seek(f,1048576-256,ASTER_FS_SEEK_SET);
    if(r<0){aster_fs_close(f);return r;}
    n=aster_fs_read(f,bytes,sizeof(bytes));r=aster_fs_close(f);
    if(r || n!=sizeof(bytes))return ASTER_ECORRUPT;
    for(unsigned i=0;i<256;i++)if(bytes[i]!=(uint8_t)((1048576-256+i)%251))return ASTER_ECORRUPT;
    aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.format=ASTER_PIXEL_RGB565};
    r=aster_surface_create(&desc,sizeof(desc),&surface);if(r)return r;
    aster_surface_buffer_t b={.size=sizeof(b),.version=1};r=aster_surface_acquire(surface,&b,sizeof(b));if(r)return r;
    aster_ge_init(&ge,surface,b.width,b.height,commands,16,arena,sizeof(arena));
    aster_ge_clear(&ge,ASTER_RGB565(242,245,250));
    aster_ge_text(&ge,24,76,24,ASTER_RGB565(25,38,58),ASTER_GFX_NOBG,"资源安装成功");
    aster_ge_text(&ge,24,124,16,ASTER_RGB565(36,102,220),ASTER_GFX_NOBG,"文本与 1 MiB 数据文件校验通过");
    aster_ge_text(&ge,24,166,14,ASTER_RGB565(111,126,145),ASTER_GFX_NOBG,"这些资源不在程序 AST 包内。");
    r=aster_ge_present(&ge);
    aster_logf(ASTER_LOG_INFO,"STOREDEMO PASS external message and 1048576-byte resource");
    aster_app_set_framerate(10);return r<0?r:0;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e){(void)e;}
ASTER_APP_EXPORT void aster_app_stop(void){if(surface)aster_surface_destroy(surface);surface=0;}
