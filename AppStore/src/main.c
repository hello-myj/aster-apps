/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include "codec.h"
#ifndef STORE_URL
#define STORE_URL "http://127.0.0.1:9080"
#endif
#define PAGE 8
#define PART (8u*1024u*1024u)
typedef struct {char id[64],name[96],release[65],version[24],description[384],permissions[160];uint32_t bytes,count;} entry;
typedef struct {char path[96],sha[65];uint32_t index,bytes;int package;} artifact;
static entry entries[PAGE];static artifact files[PAGE];
static int entry_count,selected,catalog_page,list_offset,catalog_next=-1,manifest_page,file_count,file_at,manifest_next=-1;
static aster_system_info_t device;
static aster_handle_t surface,output;
static aster_ge_t ge;static aster_gfx_cmd_t commands[128];static uint8_t arena[4096];
static char server[192]=STORE_URL,edit[192],target[16],query[256],url[1024],headers[160];
static char body[16384],status_line[128],stage[64],cleanup[64],local_file[160];
static uint8_t buffer[2048];static json_doc doc;
static int request,closing,active,wanted,view,dirty=1,header_checked;
static uint32_t body_len,chunk_start,chunk_end,chunk_received,file_received,total_received,total_files,completed_files,last_draw;
static store_sha256 checksum;
enum { LIST,DETAIL,SETTINGS,DOWNLOAD,RESULT };
enum { NONE,CATALOG,MANIFEST,FILE_DATA };
static uint16_t ink,bg,muted,blue,white;
static int text(int o,const char *key,char *p,size_t n){return json_string(&doc,json_key(&doc,o,key),p,n);}
static int number(int o,const char *key,uint32_t *p){return json_uint(&doc,json_key(&doc,o,key),p);}
static void copystr(char *d,const char *s){memcpy(d,s,strlen(s)+1);}
static void appendstr(char *d,const char *s){copystr(d+strlen(d),s);}
static int equals(const char *a,const char *b){return !strcmp(a,b);}
static int hex64(const char *s){if(strlen(s)!=64)return 0;for(unsigned i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}
static int next_page(void)
{uint32_t n;if(!number(0,"next",&n))return n<=10000?(int)n:-2;int k=json_key(&doc,0,"next");return k>=0 && doc.tokens[k].end-doc.tokens[k].start==2 && !memcmp(body+doc.tokens[k].start,"-1",2)?-1:-2;}
static void draw(void);
static int is_installed(const entry *e)
{
    int count=aster_appmgr_count();
    for(int i=0;i<count && i<32;i++){aster_app_info_t info={.size=sizeof(info),.version=2};if(!aster_appmgr_list(i,&info,sizeof(info)) && equals(info.id,e->id))return 1;}
    return 0;
}
static int valid_server(const char *s)
{
    size_t n=strlen(s);if(n<8 || n>=sizeof(server))return 0;
    if(strncmp(s,"http://",7) && strncmp(s,"https://",8))return 0;
    for(size_t i=0;i<n;i++)if((unsigned char)s[i]<=32 || (unsigned char)s[i]>=127 || s[i]=='@' || s[i]=='?' || s[i]=='#')return 0;
    return 1;
}
static int relpath(const char *s)
{
    if(!s[0] || s[0]=='/' || strlen(s)>95)return 0;
    const char *part=s;
    for(const char *p=s;;p++) {
        if(*p=='\\' || *p==':' || (*p && (unsigned char)*p<32))return 0;
        if(*p=='/' || !*p){size_t n=p-part;if(!n || (n==1&&part[0]=='.') || (n==2&&part[0]=='.'&&part[1]=='.'))return 0;part=p+1;}
        if(!*p)break;
    }
    return 1;
}
static void message(const char *s,int error)
{aster_snprintf(status_line,sizeof(status_line),"%s (%d)",s,error);dirty=1;}
static void stop_network(void)
{
    wanted=NONE;active=NONE;
    if(output){aster_fs_close(output);output=0;}
    if(request>0){aster_http_cancel(request);closing=1;}
}
static void fail(const char *s,int error)
{
    stop_network();view=RESULT;message(s,error);
    if(stage[0]){copystr(cleanup,stage);stage[0]=0;}
    aster_logf(ASTER_LOG_ERROR,"STORE FAIL %s code=%d",s,error);
}
static int mkdirs(char *path)
{
    for(char *p=path+1;*p;p++)if(*p=='/') {
        *p=0;int r=aster_fs_mkdir(path,strlen(path));*p='/';if(r && r!=ASTER_EEXIST)return r;
    }
    return 0;
}
/* One deletion per frame; close the parent before recursion (four FD limit). */
static int erase_one(const char *path,unsigned depth)
{
    if(depth>16)return ASTER_EINVAL;
    aster_handle_t dir;int r=aster_fs_opendir(path,strlen(path),&dir);
    if(r==ASTER_ENOENT)return 0;if(r)return r;
    char name[128];uint32_t is_dir=0;r=aster_fs_dir_next(dir,name,sizeof(name),&is_dir);
    int close=aster_fs_closedir(dir);if(r<0 || close)return r<0?r:close;
    if(!r){r=aster_fs_unlink(path,strlen(path));return r&&r!=ASTER_ENOENT?r:0;}
    if(!relpath(name) || strchr(name,'/'))return ASTER_EINVAL;
    char child[160];if(aster_snprintf(child,sizeof(child),"%s/%s",path,name)>=(int)sizeof(child))return ASTER_EINVAL;
    r=is_dir?erase_one(child,depth+1):aster_fs_unlink(child,strlen(child));
    return r<0?r:1;
}
static void make_query(void)
{
    aster_snprintf(query,sizeof(query),"board=%s&target=%s&abi=%u&features=%u&width=%u&height=%u",
        device.board,target,device.abi_version,(uint32_t)device.feature_flags,ge.w,ge.h);
}
static void refresh(void)
{entry_count=0;list_offset=0;catalog_page=0;view=LIST;wanted=CATALOG;message("正在获取兼容应用",0);}
static int read_catalog(void)
{
    int array=json_key(&doc,0,"items");if(array<0 || doc.tokens[array].type!='[' || doc.tokens[array].count>PAGE)return -1;
    entry_count=doc.tokens[array].count;int at=array+1;
    for(int i=0;i<entry_count;i++) {
        entry *e=&entries[i];memset(e,0,sizeof(*e));
        if(text(at,"id",e->id,sizeof(e->id))<0 || text(at,"name",e->name,sizeof(e->name))<0 ||
           text(at,"release",e->release,sizeof(e->release))<0 || !hex64(e->release) ||
           text(at,"version",e->version,sizeof(e->version))<0 ||
           number(at,"bytes",&e->bytes) || number(at,"file_count",&e->count) || !e->count || e->count>4097)return -1;
        if(text(at,"description",e->description,sizeof(e->description))<0)copystr(e->description,"此应用未提供简介。");
        int perms=json_key(&doc,at,"permissions");if(perms<0 || doc.tokens[perms].type!='[')return -1;
        int p=perms+1;
        for(int j=0;j<doc.tokens[perms].count;j++) {
            char perm[24];if(json_string(&doc,p,perm,sizeof(perm))<0)return -1;
            size_t n=strlen(e->permissions);if(n+strlen(perm)+3>=sizeof(e->permissions))return -1;
            const char *display=equals(perm,"fs")?"文件读写":equals(perm,"kv")?"保存设置":equals(perm,"net")?"联网":equals(perm,"appmgr")?"应用管理":equals(perm,"sd")?"SD 卡访问":perm;
            if(n+strlen(display)+3>=sizeof(e->permissions))return -1;
            if(n)appendstr(e->permissions,", ");appendstr(e->permissions,display);p=doc.tokens[p].next;
        }
        at=doc.tokens[at].next;
    }
    catalog_next=next_page();if(catalog_next< -1)return -1;
    dirty=1;copystr(status_line,entry_count?"仅显示与此设备兼容的版本":"没有适配此设备的已发布应用");return 0;
}
static int array_contains(int id,const char *value)
{
    if(id<0 || doc.tokens[id].type!='[')return 0;int at=id+1;
    for(int i=0;i<doc.tokens[id].count;i++){char s[96];if(json_string(&doc,at,s,sizeof(s))>=0 && equals(s,value))return 1;at=doc.tokens[at].next;}return 0;
}
static int verify_release(void)
{
    char id[64],release[65],backend[24],version[24];
    if(text(0,"id",id,sizeof(id))<0 || !equals(id,entries[selected].id) ||
       text(0,"release",release,sizeof(release))<0 || !equals(release,entries[selected].release) ||
       text(0,"version",version,sizeof(version))<0 || !equals(version,entries[selected].version) ||
       text(0,"target",backend,sizeof(backend))<0 || !equals(backend,target) ||
       !array_contains(json_key(&doc,0,"boards"),device.board))return -1;
    int a=json_key(&doc,0,"abi_min");if(a<0 || doc.tokens[a].count!=3)return -1;
    uint32_t v[3];for(int i=0;i<3;i++)if(json_uint(&doc,a+1+i,&v[i]) || v[i]>255)return -1;
    uint32_t abi=(v[0]<<16)|(v[1]<<8)|v[2];
    if(v[0]!=(device.abi_version>>16) || abi>device.abi_version)return -1;
    int screens=json_key(&doc,0,"screens");if(screens<0 || doc.tokens[screens].type!='[')return -1;
    if(doc.tokens[screens].count) {
        int fit=0,at=screens+1;
        for(int i=0;i<doc.tokens[screens].count;i++) {uint32_t w,h;if(doc.tokens[at].type!='[' || doc.tokens[at].count!=2 || json_uint(&doc,at+1,&w)||json_uint(&doc,at+2,&h))return -1;if(w==ge.w&&h==ge.h)fit=1;at=doc.tokens[at].next;}
        if(!fit)return -1;
    }
    static const char *names[23]={"display","touch","mouse","keyboard","camera","audio-in","audio-out","wifi","bluetooth","accelerometer","gyroscope","filesystem","rtc","psram","framebuffer","network","http","tls","audio-stream","mqtt","websocket","audio-effects","audio-effects-memory"};
    int needs=json_key(&doc,0,"requires");if(needs<0 || doc.tokens[needs].type!='[')return -1;
    int at=needs+1;
    for(int i=0;i<doc.tokens[needs].count;i++) {
        char name[32];if(json_string(&doc,at,name,sizeof(name))<0)return -1;int bit=-1;
        for(int n=0;n<23;n++)if(equals(names[n],name))bit=n;
        if(equals(name,"storage.app"))bit=11;
        if(bit<0 || !(device.feature_flags&(UINT64_C(1)<<bit)))return -1;at=doc.tokens[at].next;
    }
    return 0;
}
static void begin_file(void);
static int read_manifest(void)
{
    if(verify_release())return -1;
    uint32_t total;if(number(0,"total",&total) || total!=entries[selected].count)return -1;
    int array=json_key(&doc,0,"files");if(array<0 || doc.tokens[array].type!='[' || doc.tokens[array].count>PAGE || !doc.tokens[array].count)return -1;
    file_count=doc.tokens[array].count;int at=array+1;uint32_t remaining=entries[selected].bytes-total_received;
    for(int i=0;i<file_count;i++) {
        artifact *f=&files[i];char role[24];
        if(text(at,"path",f->path,sizeof(f->path))<0 || !relpath(f->path) ||
           text(at,"sha256",f->sha,sizeof(f->sha))<0 || !hex64(f->sha) ||
           text(at,"role",role,sizeof(role))<0 || (!equals(role,"package") && !equals(role,"resource")) ||
           number(at,"bytes",&f->bytes) || f->bytes>1073741824u || number(at,"index",&f->index) ||
           f->index!=completed_files+(unsigned)i)return -1;
        if(f->bytes>remaining)return -1;remaining-=f->bytes;
        f->package=equals(role,"package");if(f->package!=(f->index==0))return -1;
        at=doc.tokens[at].next;
    }
    manifest_next=next_page();if(manifest_next< -1)return -1;
    file_at=0;begin_file();return 0;
}
static void file_done(void)
{
    char hash[65];sha_hex(&checksum,hash);
    if(output){int r=aster_fs_close(output);output=0;if(r){fail("关闭下载文件失败",r);return;}}
    if(!equals(hash,files[file_at].sha)){fail("文件校验失败，请重试",ASTER_ECORRUPT);return;}
    completed_files++;file_at++;dirty=1;
    if(file_at<file_count){begin_file();return;}
    if(manifest_next>=0){manifest_page=manifest_next;wanted=MANIFEST;return;}
    if(completed_files!=total_files || total_received!=entries[selected].bytes){fail("资源清单不完整",ASTER_ECORRUPT);return;}
    char package[96],assets[96];aster_snprintf(package,sizeof(package),"%s/package.ast",stage);aster_snprintf(assets,sizeof(assets),"%s/assets",stage);
    message("正在安装，完成后返回商城",0);view=RESULT;
    draw();
    int r=aster_appmgr_install(package,strlen(package),assets,strlen(assets));
    if(r)fail("无法提交安装",r);
}
static void begin_file(void)
{
    artifact *f=&files[file_at];file_received=0;sha_init(&checksum);
    int n=f->package?aster_snprintf(local_file,sizeof(local_file),"%s/package.ast",stage):
        aster_snprintf(local_file,sizeof(local_file),"%s/assets/%s",stage,f->path);
    if(n<0 || n>=(int)sizeof(local_file)){fail("资源路径过长",ASTER_EINVAL);return;}
    int r=mkdirs(local_file);if(r){fail("创建资源目录失败",r);return;}
    r=aster_fs_open(local_file,strlen(local_file),ASTER_FS_O_CREAT|ASTER_FS_O_TRUNC|ASTER_FS_O_WRONLY,&output);
    if(r){fail("无法写入 SD 卡",r);return;}
    if(!f->bytes){file_done();return;}
    wanted=FILE_DATA;
}
static void begin_install(void)
{
    if(cleanup[0] || request || wanted)return;
    if(equals(entries[selected].id,"aster.store")){message("商城自身更新请使用设备维护工具",ASTER_ENOTSUP);return;}
    uint32_t nonce=aster_time_now_ms();aster_stat_t st={.size=sizeof(st),.version=1};int unique=0;
    for(unsigned i=0;i<128;i++){aster_snprintf(stage,sizeof(stage),"/data/s%08x%02x",nonce,i);if(aster_fs_stat(stage,strlen(stage),&st,sizeof(st))==ASTER_ENOENT){unique=1;break;}}
    if(!unique){fail("暂存目录不可用",ASTER_EIO);return;}
    int r=aster_fs_mkdir(stage,strlen(stage));if(r){fail("无法创建暂存目录",r);return;}
    char assets[96];aster_snprintf(assets,sizeof(assets),"%s/assets",stage);r=aster_fs_mkdir(assets,strlen(assets));
    if(r || (r=aster_kv_set("store.stage",11,stage,strlen(stage)+1))){fail("无法保存下载状态",r);return;}
    total_received=completed_files=0;total_files=entries[selected].count;
    manifest_page=0;view=DOWNLOAD;wanted=MANIFEST;message("正在下载并校验资源",0);
}
static int start_request(void)
{
    headers[0]=0;body_len=0;header_checked=0;active=wanted;
    if(active==CATALOG)aster_snprintf(url,sizeof(url),"%s/v1/catalog?%s&page=%d",server,query,catalog_page);
    else if(active==MANIFEST)aster_snprintf(url,sizeof(url),"%s/v1/releases/%s?%s&page=%d",server,entries[selected].release,query,manifest_page);
    else {
        artifact *f=&files[file_at];chunk_start=file_received;chunk_end=f->bytes-1;
        if(chunk_end-chunk_start>=PART)chunk_end=chunk_start+PART-1;
        chunk_received=0;
        aster_snprintf(url,sizeof(url),"%s/v1/releases/%s/files/%u?%s",server,entries[selected].release,f->index,query);
        aster_snprintf(headers,sizeof(headers),"Range: bytes=%u-%u\r\nIf-Range: \"%s\"\r\n",chunk_start,chunk_end,f->sha);
    }
    int r=aster_http_request(ASTER_HTTP_METHOD_GET,url,strlen(url),headers[0]?headers:NULL,strlen(headers),NULL,0,600000,active==FILE_DATA?PART:sizeof(body)-1);
    if(r==ASTER_EAGAIN){active=NONE;return 0;}if(r<0)return r;
    request=r;wanted=NONE;return 0;
}
static int check_headers(const aster_http_status_t *status)
{
    if(!(status->flags&ASTER_HTTP_FLAG_HEADERS))return ASTER_EAGAIN;
    if(active!=FILE_DATA)return status->status==200?0:ASTER_EPROTO;
    if(status->status!=206 || !(status->flags&ASTER_HTTP_FLAG_LENGTH) ||
       status->content_length!=chunk_end-chunk_start+1)return ASTER_EPROTO;
    char got[128],expected[128];uint32_t required;
    int r=aster_http_header(request,"Content-Range",13,0,got,sizeof(got)-1,&required);if(r<0)return r;got[r]=0;
    aster_snprintf(expected,sizeof(expected),"bytes %u-%u/%u",chunk_start,chunk_end,files[file_at].bytes);
    if(!equals(got,expected))return ASTER_EPROTO;
    r=aster_http_header(request,"ETag",4,0,got,sizeof(got)-1,&required);if(r<0)return r;got[r]=0;
    aster_snprintf(expected,sizeof(expected),"\"%s\"",files[file_at].sha);return equals(got,expected)?0:ASTER_EPROTO;
}
static void network_tick(void)
{
    if(closing){int r=aster_http_close(request);if(r==ASTER_EAGAIN)return;request=closing=0;}
    if(!request && wanted){int r=start_request();if(r){fail("无法连接服务器",r);return;}}
    if(!request)return;
    aster_http_status_t status;int r=aster_http_status(request,&status,sizeof(status));
    if(r==ASTER_EAGAIN)return;if(r){fail("读取网络状态失败",r);return;}
    if(status.state==ASTER_HTTP_STATE_FAILED || status.state==ASTER_HTTP_STATE_CANCELLED){fail("网络连接失败",status.error?status.error:ASTER_EIO);return;}
    if(!header_checked){r=check_headers(&status);if(r==ASTER_EAGAIN)return;if(r){fail("服务器响应不匹配",r);return;}header_checked=1;}
    uint32_t started=aster_time_now_ms();
    for(unsigned i=0;i<16;i++) {
        r=aster_http_read(request,buffer,sizeof(buffer));if(r==ASTER_EAGAIN)return;if(r<0){fail("下载中断，请重试",r);return;}
        if(!r) {
            if(aster_http_status(request,&status,sizeof(status)) || status.state!=ASTER_HTTP_STATE_DONE){fail("下载尚未完成",ASTER_EPROTO);return;}
            closing=1;
            if(active==FILE_DATA) {
                if(chunk_received!=chunk_end-chunk_start+1){fail("下载长度错误",ASTER_ECORRUPT);return;}
                active=NONE;if(file_received==files[file_at].bytes)file_done();else wanted=FILE_DATA;
            } else {
                int kind=active;active=NONE;body[body_len]=0;
                uint32_t schema;
                if(json_parse(&doc,body,body_len) || number(0,"schema",&schema) || schema!=1 ||
                   (kind==CATALOG?read_catalog():read_manifest()))fail("无效的商城数据",ASTER_ECORRUPT);
            }
            return;
        }
        if(active==FILE_DATA) {
            if((uint32_t)r>chunk_end-chunk_start+1-chunk_received || (uint32_t)r>files[file_at].bytes-file_received){fail("文件超出声明长度",ASTER_ECORRUPT);return;}
            unsigned written=0;
            while(written<(unsigned)r){int n=aster_fs_write(output,buffer+written,(unsigned)r-written);if(n<=0 || (unsigned)n>(unsigned)r-written){fail("SD 卡写入失败或空间不足",n);return;}written+=(unsigned)n;}
            sha_update(&checksum,buffer,(unsigned)r);file_received+=(unsigned)r;chunk_received+=(unsigned)r;total_received+=(unsigned)r;dirty=1;
        } else {
            if(body_len+(unsigned)r>=sizeof(body)){fail("商城数据过大",ASTER_EMSGSIZE);return;}
            memcpy(body+body_len,buffer,(unsigned)r);body_len+=(unsigned)r;
        }
        if(aster_time_now_ms()-started>=4)return;
    }
}
static void label(int x,int y,unsigned size,uint16_t color,const char *s)
{aster_ge_text(&ge,x,y+aster_text_asc(size),size,color,ASTER_GFX_NOBG,s);}
static void button(int x,int y,int w,int h,const char *s,int enabled)
{aster_ge_rect_r(&ge,x,y,w,h,enabled?blue:muted,8,0);label(x+10,y+(h-16)/2,16,white,s);}
static int compact(void){return ge.h<300;}
static int visible(void){int n=compact()?((int)ge.h-118)/50:((int)ge.h-164)/72;return n<1?1:n>6?6:n;}
static int key_step(void){int step=((int)ge.h-56-52-128)/4;return step>40?40:step;}
/* Compact geometry is shared with hit testing; the larger layout stays intact. */
static const char keyboard[]="1234567890qwertyuiopasdfghjklzxcvbnm.:/-_";
static void paragraph(int y,unsigned size,const char *p,int lines)
{
    for(int line=0;*p && line<lines;line++) {
        char row[128];unsigned n=0,chars=0,max=(ge.w-36)/size;
        while(*p && *p!='\n' && chars<max && n<sizeof(row)-5){unsigned bytes=((unsigned char)*p<128)?1:((unsigned char)*p<224)?2:((unsigned char)*p<240)?3:4;for(unsigned k=0;k<bytes&&*p;k++)row[n++]=*p++;chars++;}
        if(*p=='\n')p++;row[n]=0;label(18,y,size,ink,row);y+=size+4;
    }
}
static void draw_compact(void)
{
    char s[192];int bottom=ge.h-44,keyw=(ge.w-24)/12;
    label(12,8,20,ink,"Aster 商城");
    aster_snprintf(s,sizeof(s),"%s · %s",device.board,target);label(12,34,10,muted,s);
    if(view==SETTINGS) {
        size_t len=strlen(edit);unsigned shown=(ge.w-36)/7;
        label(18,57,12,blue,edit+(len>shown?len-shown:0));
        for(unsigned i=0;i<sizeof(keyboard)-1;i++){char t[2]={keyboard[i],0};int x=12+(i%12)*keyw,y=82+(i/12)*26;aster_ge_rect_r(&ge,x,y,keyw-3,23,blue,3,0);label(x+4,y+3,12,white,t);}
        button(12+5*keyw,160,3*keyw-3,23,"删除",1);button(12+8*keyw,160,4*keyw-3,23,"清空",1);
        button(12,bottom,90,36,"返回",1);button(110,bottom,ge.w-122,36,"保存并连接",valid_server(edit));
    } else if(view==LIST) {
        label(12,56,11,muted,status_line);
        for(int row=0;row<visible() && list_offset+row<entry_count;row++) {
            entry *e=&entries[list_offset+row];int y=74+row*50;
            aster_ge_rect_r(&ge,12,y,ge.w-24,44,white,6,0);
            label(20,y+3,16,ink,e->name);aster_snprintf(s,sizeof(s),"v%s · %u KiB%s",e->version,e->bytes/1024,is_installed(e)?" · 已安装":"");label(20,y+25,10,muted,s);
        }
        int w=(ge.w-28)/4;
        button(8,bottom,w,36,"设置",!request);button(12+w,bottom,w,36,"刷新",!request);
        button(16+2*w,bottom,w,36,"上页",list_offset>0||catalog_page>0);button(20+3*w,bottom,w,36,"下页",list_offset+visible()<entry_count||catalog_next>=0);
    } else if(view==DETAIL) {
        entry *e=&entries[selected];label(18,60,18,ink,e->name);
        aster_snprintf(s,sizeof(s),"v%s · %u KiB · %u 个文件",e->version,e->bytes/1024,e->count);label(18,84,11,muted,s);
        paragraph(105,12,e->description,2);label(18,140,10,muted,"所需权限");paragraph(154,11,e->permissions[0]?e->permissions:"无额外权限",2);
        button(12,bottom,62,36,"返回",1);button(80,bottom,62,36,"打开",is_installed(e));button(148,bottom,ge.w-160,36,"下载安装",!cleanup[0]&&!request);
    } else if(view==DOWNLOAD) {
        label(18,62,18,ink,entries[selected].name);label(18,90,12,ink,"下载并校验中");
        unsigned progress=entries[selected].bytes?(uint64_t)total_received*100/entries[selected].bytes:0;
        aster_ge_rect_r(&ge,18,116,ge.w-36,10,white,5,0);aster_ge_rect_r(&ge,18,116,(ge.w-36)*progress/100,10,blue,5,0);
        aster_snprintf(s,sizeof(s),"%u%% · %u / %u KiB",progress,total_received/1024,entries[selected].bytes/1024);label(18,137,12,ink,s);
        aster_snprintf(s,sizeof(s),"已校验 %u / %u 个文件",completed_files,total_files);label(18,158,11,muted,s);
        button(18,bottom,ge.w-36,36,"取消下载",1);
    } else {
        label(18,78,18,ink,"操作结果");paragraph(112,12,status_line,3);button(18,bottom,ge.w-36,36,"返回商城",1);
    }
}
static void tap_compact(int x,int y)
{
    int bottom=ge.h-44;
    if(view==SETTINGS) {
        int keyw=(ge.w-24)/12;
        if(y>=82 && y<186 && x>=12 && x<12+12*keyw) {
            unsigned col=(x-12)/keyw,row=(y-82)/26,i=row*12+col;
            if(row==3 && col>=5){if(col<8){size_t n=strlen(edit);if(n)edit[n-1]=0;}else edit[0]=0;}
            else if(i<sizeof(keyboard)-1 && strlen(edit)+1<sizeof(edit)){size_t n=strlen(edit);edit[n]=keyboard[i];edit[n+1]=0;}
        } else if(y>=bottom && y<bottom+36){if(x<104)view=LIST;else if(valid_server(edit)){copystr(server,edit);size_t n=strlen(server);while(n>8&&server[n-1]=='/')server[--n]=0;aster_kv_set("store.url",9,server,n+1);refresh();}}
    } else if(view==LIST) {
        if(y>=bottom && y<bottom+36) {
            int col=(x-8)/(((int)ge.w-28)/4+4);
            if(!request && col==0){copystr(edit,server);view=SETTINGS;}
            else if(!request && col==1)refresh();
            else if(!request && col==2){if(list_offset>0)list_offset-=visible();else if(catalog_page>0){catalog_page--;wanted=CATALOG;}if(list_offset<0)list_offset=0;}
            else if(!request && col==3){if(list_offset+visible()<entry_count)list_offset+=visible();else if(catalog_next>=0){catalog_page=catalog_next;list_offset=0;wanted=CATALOG;}}
        } else if(y>=74 && y<74+visible()*50){int index=list_offset+(y-74)/50;if(index<entry_count){selected=index;view=DETAIL;}}
    } else if(view==DETAIL && y>=bottom && y<bottom+36){if(x<77)view=LIST;else if(x<145){if(is_installed(&entries[selected])){int r=aster_appmgr_start(entries[selected].id,strlen(entries[selected].id));if(r)message("打开失败",r);}}else begin_install();}
    else if(view==DOWNLOAD && y>=bottom && y<bottom+36)fail("下载已取消",0);
    else if(view==RESULT && y>=bottom && y<bottom+36){view=LIST;if(!request)refresh();}
}
static void draw(void)
{
    aster_ge_clear(&ge,bg);
    if(compact()){draw_compact();goto present;}
    label(18,16,24,ink,"Aster 商城");
    char s[192];aster_snprintf(s,sizeof(s),"%s · %s",device.board,target);label(18,49,12,muted,s);
    int bottom=(int)ge.h-56;
    if(view==SETTINGS) {
        label(18,77,16,ink,"服务器地址");size_t len=strlen(edit);unsigned shown=(ge.w-36)/7;label(18,103,12,blue,edit+(len>shown?len-shown:0));
        const char keys[]="1234567890qwertyuiopasdfghjklzxcvbnm.:/-_";
        int keyw=((int)ge.w-24)/12,step=key_step();
        for(unsigned i=0;i<sizeof(keys)-1;i++){char t[2]={keys[i],0};int x=12+(i%12)*keyw,y=128+(i/12)*step;aster_ge_rect_r(&ge,x,y,keyw-3,step-3,blue,3,0);label(x+4,y+1,12,white,t);}
        button(18,bottom-52,90,40,"删除",1);button(118,bottom-52,90,40,"清空",1);
        button(18,bottom,90,40,"返回",1);button(118,bottom,(int)ge.w-136,40,"保存并连接",valid_server(edit));
    } else if(view==LIST) {
        label(18,79,12,muted,status_line);
        int n=visible();for(int row=0;row<n && list_offset+row<entry_count;row++) {
            entry *e=&entries[list_offset+row];int y=104+row*72;
            aster_ge_rect_r(&ge,12,y,ge.w-24,64,white,10,0);
            label(24,y+8,18,ink,e->name);aster_snprintf(s,sizeof(s),"v%s · %u KiB%s",e->version,e->bytes/1024,is_installed(e)?" · 已安装":"");label(24,y+36,12,muted,s);
            label(ge.w-52,y+23,14,blue,"查看");
        }
        button(12,bottom,66,40,"设置",!request);button(84,bottom,66,40,"刷新",!request);
        button(156,bottom,66,40,"上页",list_offset>0||catalog_page>0);
        button(228,bottom,(int)ge.w-240,40,"下页",list_offset+n<entry_count||catalog_next>=0);
    } else if(view==DETAIL) {
        entry *e=&entries[selected];label(18,92,22,ink,e->name);
        aster_snprintf(s,sizeof(s),"版本 %s · 下载 %u KiB · %u 个文件",e->version,e->bytes/1024,e->count);label(18,128,12,muted,s);
        /* Bounded lines; UTF-8 glyph boundaries are preserved. */
        const char *p=e->description;int y=164;
        for(int line=0;*p && line<(ge.h>=400?3:1);line++) {char row[96];unsigned n=0,chars=0,max=(ge.w-36)/16;while(*p && *p!='\n' && chars<max && n<sizeof(row)-5){unsigned bytes=((unsigned char)*p<128)?1:((unsigned char)*p<224)?2:((unsigned char)*p<240)?3:4;for(unsigned k=0;k<bytes&&*p;k++)row[n++]=*p++;chars++;}if(*p=='\n')p++;row[n]=0;label(18,y,16,ink,row);y+=25;}
        label(18,y+12,14,muted,"所需权限");label(18,y+38,12,ink,e->permissions[0]?e->permissions:"无额外权限");
        if(ge.h>=400)label(18,bottom-34,12,muted,cleanup[0]?"正在清理上次下载…":"程序及资源校验完成后安装");
        button(18,bottom,70,40,"返回",1);button(96,bottom,70,40,"打开",is_installed(e));button(174,bottom,(int)ge.w-192,40,"下载安装",!cleanup[0]&&!request);
    } else if(view==DOWNLOAD) {
        label(18,ge.h>=400?100:92,22,ink,entries[selected].name);label(18,ge.h>=400?145:128,16,ink,"下载并校验中");
        uint32_t total=entries[selected].bytes;unsigned progress=total?(uint64_t)total_received*100/total:0;
        int bar_y=ge.h>=400?195:167;
        aster_ge_rect_r(&ge,18,bar_y,ge.w-36,12,white,6,0);aster_ge_rect_r(&ge,18,bar_y,(ge.w-36)*progress/100,12,blue,6,0);
        aster_snprintf(s,sizeof(s),"%u%% · %u / %u KiB",progress,total_received/1024,total/1024);label(18,ge.h>=400?230:194,16,ink,s);
        aster_snprintf(s,sizeof(s),"已校验 %u / %u 个文件",completed_files,total_files);label(18,ge.h>=400?265:224,14,muted,s);
        if(ge.h>=400)label(18,bottom-34,12,muted,"安装前不会更改现有版本");button(18,bottom,ge.w-36,40,"取消下载",1);
    } else {
        label(18,112,20,ink,"操作结果");label(18,162,14,muted,status_line);
        button(18,bottom,ge.w-36,40,"返回商城",1);
    }
present:;
    int r=aster_ge_present(&ge);if(r<0)aster_logf(ASTER_LOG_ERROR,"STORE draw error=%d",r);
    dirty=0;last_draw=aster_time_now_ms();
}
static void tap(int x,int y)
{
    if(x<0 || y<0 || (unsigned)x>=ge.w || (unsigned)y>=ge.h)return;
    if(compact()){tap_compact(x,y);dirty=1;return;}
    int bottom=(int)ge.h-56;
    if(view==SETTINGS) {
        const char keys[]="1234567890qwertyuiopasdfghjklzxcvbnm.:/-_";int keyw=((int)ge.w-24)/12,step=key_step();
        if(y>=128 && y<128+4*step && x>=12){unsigned col=(x-12)/keyw,row=(y-128)/step,i=row*12+col;if(col<12 && i<sizeof(keys)-1 && strlen(edit)+1<sizeof(edit)){size_t n=strlen(edit);edit[n]=keys[i];edit[n+1]=0;}}
        else if(y>=bottom-52 && y<bottom-12){if(x<108){size_t n=strlen(edit);if(n)edit[n-1]=0;}else if(x<208)edit[0]=0;}
        else if(y>=bottom){if(x<108)view=LIST;else if(valid_server(edit)){copystr(server,edit);size_t n=strlen(server);while(n>8&&server[n-1]=='/')server[--n]=0;aster_kv_set("store.url",9,server,n+1);refresh();}}
    } else if(view==LIST) {
        if(y>=bottom) {
            if(x<78 && !request){copystr(edit,server);view=SETTINGS;}
            else if(x<150 && !request)refresh();
            else if(x<222 && !request){if(list_offset>0)list_offset-=visible();else if(catalog_page>0){catalog_page--;wanted=CATALOG;}if(list_offset<0)list_offset=0;}
            else if(!request){if(list_offset+visible()<entry_count)list_offset+=visible();else if(catalog_next>=0){catalog_page=catalog_next;list_offset=0;wanted=CATALOG;}}
        } else if(y>=104) {int index=list_offset+(y-104)/72;if(index<entry_count && index<list_offset+visible()){selected=index;view=DETAIL;}}
    } else if(view==DETAIL && y>=bottom){if(x<88)view=LIST;else if(x<166){if(is_installed(&entries[selected])){int r=aster_appmgr_start(entries[selected].id,strlen(entries[selected].id));if(r)message("打开失败",r);}}else begin_install();}
    else if(view==DOWNLOAD && y>=bottom)fail("下载已取消",0);
    else if(view==RESULT && y>=bottom){view=LIST;if(!request)refresh();}
    dirty=1;
}
ASTER_APP_EXPORT int aster_app_init(void)
{
    device.size=sizeof(device);device.version=1;int r=aster_system_get_info(&device,sizeof(device));if(r)return r;
    copystr(target,equals(device.platform,"simulator")?"wasm":device.cpu_arch==ASTER_ARCH_RISCV?"riscv":device.cpu_arch==ASTER_ARCH_XTENSA?"xtensa":device.cpu_arch==ASTER_ARCH_X86?"x86_64":"unknown");
    if(equals(target,"unknown") || !device.board[0])return ASTER_ENOTSUP;
    aster_display_info_t display={.size=sizeof(display),.version=1};
    r=aster_display_get_info(0,&display,sizeof(display));if(r)return r;
    unsigned width=display.app_width,height=display.app_height;if(width<height){unsigned swap=width;width=height;height=swap;}
    aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=width,.height=height,.format=ASTER_PIXEL_RGB565};
    r=aster_surface_create(&desc,sizeof(desc),&surface);if(r)return r;
    aster_surface_buffer_t b={.size=sizeof(b),.version=1};r=aster_surface_acquire(surface,&b,sizeof(b));if(r)return r;
    if(b.width<320 || b.height<240)return ASTER_ENOTSUP;
    aster_ge_init(&ge,surface,b.width,b.height,commands,128,arena,sizeof(arena));
    ink=ASTER_RGB565(25,38,58);bg=ASTER_RGB565(242,245,250);muted=ASTER_RGB565(111,126,145);blue=ASTER_RGB565(36,102,220);white=ASTER_RGB565(255,255,255);
    char saved[192];r=aster_kv_get("store.url",9,saved,sizeof(saved));if(r>0 && r<=(int)sizeof(saved) && saved[r-1]==0 && valid_server(saved))copystr(server,saved);
    r=aster_kv_get("store.stage",11,cleanup,sizeof(cleanup));if(r<=0 || r>(int)sizeof(cleanup) || cleanup[r-1] || strncmp(cleanup,"/data/s",7) || strchr(cleanup+6,'/'))cleanup[0]=0;
    make_query();refresh();
    int32_t installed;r=aster_kv_get("store.install.result",20,&installed,sizeof(installed));
    if(r==sizeof(installed)){view=RESULT;message(installed==0?"应用与资源已安装":installed==ASTER_EAGAIN?"上次安装未完成，请重试":"安装失败，原版本仍保留",installed);wanted=NONE;aster_kv_delete("store.install.result",20);}
    aster_app_set_framerate(60);aster_logf(ASTER_LOG_INFO,"STORE ready board=%s target=%s abi=%u",device.board,target,device.abi_version);return 0;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *event)
{
    if(event->type==ASTER_EVENT_TOUCH && event->u.touch.action==ASTER_TOUCH_UP && event->u.touch.slot==0)tap(event->u.touch.x,event->u.touch.y);
    if(event->type!=ASTER_EVENT_FRAME)return;
    network_tick();
    if(cleanup[0] && !request && !output){int r=erase_one(cleanup,0);if(!r){cleanup[0]=0;aster_kv_delete("store.stage",11);dirty=1;}else if(r<0){message("清理暂存文件失败",r);cleanup[0]=0;}}
    if(dirty && aster_time_now_ms()-last_draw>=100)draw();
}
ASTER_APP_EXPORT void aster_app_stop(void)
{if(request>0)aster_http_cancel(request);if(output)aster_fs_close(output);if(surface)aster_surface_destroy(surface);output=surface=0;}

