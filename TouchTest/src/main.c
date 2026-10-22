#include <aster_imports.h>
#include <aster_gfx_engine.h>

#define CONTACTS 10
#define TRAIL 24
static aster_handle_t surface = ASTER_HANDLE_INVALID;
static aster_ge_t ge;
static aster_gfx_cmd_t commands[1024];
static uint8_t arena[4096];
static uint32_t width, height, reported_max, peak, downs, moves, ups, anomalies, dropped;
static uint64_t now;
static int dirty = 1, reset_slot = -1, reset_valid;
static int32_t reset_x, reset_y;
static const uint16_t palette[CONTACTS] = {0x07ff,0xfb8e,0xffe0,0x87f0,0xbdff,0xfd20,0xf81f,0x7dff,0xffff,0xcfe0};
typedef struct { int32_t x,y; } Point;
typedef struct {
    uint32_t slot;
    int used, active, head, length;
    int32_t x,y;
    uint64_t released;
    Point trail[TRAIL];
} Contact;
static Contact contacts[CONTACTS];
static uint32_t active_count(void) {
    uint32_t n=0;
    for(int i=0;i<CONTACTS;i++) n+=contacts[i].active!=0;
    return n;
}
static uint32_t root(uint32_t n) {
    uint32_t r=0, bit=1u<<30;
    while(bit>n) bit>>=2;
    while(bit) { if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2; }
    return r;
}
static int text(int x,int y,int px,uint16_t color,const char *s) {
    return aster_ge_text(&ge,x,y+aster_text_asc(px),px,color,ASTER_GFX_NOBG,s);
}
static int in_reset(int x,int y) { return x>=(int)width-88 && y>=8 && y<48; }
static void clear_contacts(void) {
    memset(contacts,0,sizeof(contacts)); reset_slot=-1; reset_valid=0; dirty=1;
}
static void release(void) {
    clear_contacts();
    if(surface!=ASTER_HANDLE_INVALID){aster_surface_destroy(surface);surface=ASTER_HANDLE_INVALID;}
}
static int find_contact(uint32_t slot,int create) {
    int free_slot=-1;
    for(int i=0;i<CONTACTS;i++) {
        if(contacts[i].used && contacts[i].slot==slot) return i;
        if(!contacts[i].active && (free_slot<0 || contacts[i].released<contacts[free_slot].released)) free_slot=i;
    }
    if(!create || free_slot<0) return -1;
    memset(&contacts[free_slot],0,sizeof(Contact));
    contacts[free_slot].used=1;contacts[free_slot].slot=slot;
    return free_slot;
}
static void append(Contact *c,int x,int y) {
    c->x=x;c->y=y;
    if(c->length) {
        Point last=c->trail[(c->head+TRAIL-1)%TRAIL];
        if(last.x==x && last.y==y) return;
    }
    c->trail[c->head]=(Point){x,y};c->head=(c->head+1)%TRAIL;
    if(c->length<TRAIL)c->length++;
}
static int render(void) {
    char b[120];
    const uint16_t bg=ASTER_RGB565(8,15,26),grid=ASTER_RGB565(20,34,48);
    aster_ge_clear(&ge,bg);
    for(uint32_t x=0;x<width;x+=32)aster_ge_line_aa(&ge,x,0,x,height-1,grid);
    for(uint32_t y=0;y<height;y+=32)aster_ge_line_aa(&ge,0,y,width-1,y,grid);
    int a=-1,bslot=-1;
    for(int i=0;i<CONTACTS;i++)if(contacts[i].active){if(a<0)a=i;else if(bslot<0)bslot=i;}
    if(active_count()==2 && a>=0 && bslot>=0) {
        Contact *c=&contacts[a],*d=&contacts[bslot];
        int dx=c->x-d->x,dy=c->y-d->y;
        aster_ge_line_aa(&ge,c->x,c->y,d->x,d->y,ASTER_RGB565(74,101,126));
        aster_snprintf(b,sizeof(b),"双指距离 %u px",root((uint32_t)(dx*dx+dy*dy)));
        text(14,height-68,16,0xbdf7,b);
    }
    aster_ge_rect(&ge,0,0,width,58,ASTER_RGB565(13,24,40));
    text(14,8,20,ASTER_WHITE,"多指触控测试");
    aster_snprintf(b,sizeof(b),"当前 %u   最高 %u   设备上限 %u",active_count(),peak,reported_max);
    text(14,34,16,0x9cf3,b);
    aster_ge_rect_r(&ge,width-88,8,76,40,reset_valid?0x2b0c:0x1948,8,0);
    text(width-68,19,16,ASTER_WHITE,"重置");
    if(!active_count())text(14,76,16,0x7bef,"请用多根手指同时按住、移动或交叉");
    for(int i=0;i<CONTACTS;i++) {
        Contact *c=&contacts[i];
        if(!c->used || (!c->active && now-c->released>700))continue;
        uint16_t color=palette[i];
        int first=(c->head+TRAIL-c->length)%TRAIL;
        for(int j=1;j<c->length;j++){
            Point p=c->trail[(first+j-1)%TRAIL],q=c->trail[(first+j)%TRAIL];
            aster_ge_line_aa(&ge,p.x,p.y,q.x,q.y,color);
        }
        aster_ge_ellipse(&ge,c->x,c->y,c->active?28:16,c->active?28:16,color,1);
        if(c->active) {
            aster_ge_ellipse(&ge,c->x,c->y,24,24,color,1);
            aster_ge_line_aa(&ge,c->x-9,c->y,c->x+9,c->y,color);
            aster_ge_line_aa(&ge,c->x,c->y-9,c->x,c->y+9,color);
        }
        aster_snprintf(b,sizeof(b),"ID %u%s",c->slot,c->active?"":" UP");
        int lx=c->x+32,ly=c->y-26;
        if(lx>(int)width-116)lx=c->x-116;
        if(lx<4)lx=4;
        if(ly<60)ly=60;
        if(ly>(int)height-70)ly=height-70;
        text(lx,ly,16,color,b);
        aster_snprintf(b,sizeof(b),"%d, %d",c->x,c->y);
        text(lx,ly+19,16,color,b);
    }
    aster_ge_rect(&ge,0,height-40,width,40,ASTER_RGB565(13,24,40));
    aster_snprintf(b,sizeof(b),"DOWN %u  MOVE %u  UP %u",downs,moves,ups);
    text(12,height-38,16,0xbdf7,b);
    aster_snprintf(b,sizeof(b),"异常 %u  超容量 %u  |  所有区域均记录触点",anomalies,dropped);
    text(12,height-20,16,0x7bef,b);
    return aster_ge_present(&ge);
}
static void touch(const aster_event_t *e) {
    uint32_t action=e->u.touch.action, slot=e->u.touch.slot;
    int x=e->u.touch.x<(uint32_t)width?(int)e->u.touch.x:(int)width-1;
    int y=e->u.touch.y<(uint32_t)height?(int)e->u.touch.y:(int)height-1;
    int i=find_contact(slot,action==ASTER_TOUCH_DOWN || action==ASTER_TOUCH_MOVE);
    if(i<0){if(action==ASTER_TOUCH_UP)anomalies++;else dropped++;dirty=1;return;}
    Contact *c=&contacts[i];
    if(action==ASTER_TOUCH_DOWN) {
        downs++;
        if(c->active)anomalies++;
        c->active=1;c->length=0;c->head=0;
        append(c,x,y);
        if(active_count()==1 && in_reset(x,y)) {reset_slot=i;reset_valid=1;reset_x=x;reset_y=y;}
        if(active_count()>1)reset_valid=0;
        aster_printf("touch DOWN slot=%u x=%d y=%d active=%u",slot,x,y,active_count());
    } else if(action==ASTER_TOUCH_MOVE) {
        moves++;
        if(!c->active){anomalies++;c->active=1;c->length=0;c->head=0;reset_valid=0;}
        append(c,x,y);
        if(i==reset_slot && (x-reset_x>12 || reset_x-x>12 || y-reset_y>12 || reset_y-y>12))reset_valid=0;
    } else if(action==ASTER_TOUCH_UP) {
        ups++;
        if(!c->active)anomalies++;
        append(c,x,y);c->active=0;c->released=now;
        aster_printf("touch UP slot=%u x=%d y=%d active=%u",slot,x,y,active_count());
        if(i==reset_slot) {
            if(reset_valid && in_reset(x,y) && !active_count()){
                downs=moves=ups=anomalies=dropped=peak=0;clear_contacts();
            }
            reset_slot=-1;reset_valid=0;
        }
    } else return;
    uint32_t n=active_count();if(n>peak)peak=n;
    dirty=1;
}
ASTER_APP_EXPORT int aster_app_init(void) {
    aster_display_info_t d={.size=sizeof(d),.version=1};
    int32_t rc=aster_display_get_info(0,&d,sizeof(d));if(rc<0)return rc;
    width=d.app_width;height=d.app_height;
    if(width<320 || height<240 || width>1024 || height>1024)return ASTER_ENOTSUP;
    aster_input_info_t input={.size=sizeof(input),.version=1};
    rc=aster_input_get_info(&input,sizeof(input));if(rc<0)return rc;
    reported_max=input.max_points;
    aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=width,.height=height,.format=ASTER_PIXEL_RGB565,.flags=0};
    rc=aster_surface_create(&desc,sizeof(desc),&surface);if(rc<0)return rc;
    aster_ge_init(&ge,surface,width,height,commands,1024,arena,sizeof(arena));
    clear_contacts();
    rc=aster_app_set_framerate(30);if(rc<0){release();return rc;}
    aster_printf("TouchTest %ux%u reported_max=%u tracked_capacity=%u",width,height,reported_max,CONTACTS);
    return ASTER_OK;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e) {
    if(!e || surface==ASTER_HANDLE_INVALID)return;
    now=e->timestamp_ms;
    if(e->type==ASTER_EVENT_TOUCH)touch(e);
    else if(e->type==ASTER_EVENT_FRAME) {
        for(int i=0;i<CONTACTS;i++)if(contacts[i].used && !contacts[i].active && now-contacts[i].released>700){contacts[i].used=0;dirty=1;}
        int fonts_changed=aster_gfx_fonts_pending();
        if(dirty || fonts_changed){int rc=render();if(rc<0){aster_errfmt("render %d",rc);aster_app_exit(rc);return;}dirty=0;}
    } else if(e->type==ASTER_EVENT_KEY && e->u.key.pressed) {
        if(e->u.key.code==1){aster_app_exit(0);return;}
        if(e->u.key.code==57){downs=moves=ups=anomalies=dropped=0;peak=active_count();dirty=1;}
    }
}
ASTER_APP_EXPORT void aster_app_pause(void){clear_contacts();}
ASTER_APP_EXPORT void aster_app_resume(void){clear_contacts();}
ASTER_APP_EXPORT void aster_app_stop(void){release();}
ASTER_APP_EXPORT void aster_app_destroy(void){release();}