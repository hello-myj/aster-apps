/* libc compatibility backend. No WASI host imports are permitted in the final APP.
 * All actual I/O terminates at the documented Aster ABI. */
#include <aster_imports.h>
#include <wasi/api.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#define FDCAP 32
__attribute__((noreturn)) void tyrian_exit(int code);
typedef struct {aster_handle_t h;int used,dir;uint64_t pos,cookie;char path[512],pending[256];uint32_t pendingDir;int hasPending;} FD;
static FD fds[FDCAP];
static __wasi_errno_t err(int rc){switch(rc){case ASTER_ENOENT:return __WASI_ERRNO_NOENT;case ASTER_EEXIST:return __WASI_ERRNO_EXIST;case ASTER_ENOMEM:return __WASI_ERRNO_NOMEM;case ASTER_EPERM:return __WASI_ERRNO_ACCES;case ASTER_EINVAL:return __WASI_ERRNO_INVAL;default:return rc>=0?0:__WASI_ERRNO_IO;}}
static int valid(unsigned fd){return fd<FDCAP&&(fd==3||fds[fd].used);}
static int pathname(unsigned fd,const char *s,char out[512]){if(!s||!valid(fd))return 0;const char *base=fd==3?"/data":fds[fd].path;if(s[0]=='/')snprintf(out,512,"%s",s);else snprintf(out,512,"%s/%s",base,s);return !strncmp(out,"/data",5)&&!strstr(out,"/../")&&!strstr(out,"/..\0");}
__wasi_errno_t __wasi_fd_prestat_get(__wasi_fd_t fd,__wasi_prestat_t *v){if(fd!=3)return __WASI_ERRNO_BADF;memset(v,0,sizeof(*v));v->tag=__WASI_PREOPENTYPE_DIR;v->u.dir.pr_name_len=5;return 0;}
__wasi_errno_t __wasi_fd_prestat_dir_name(__wasi_fd_t fd,uint8_t *b,__wasi_size_t n){if(fd!=3)return __WASI_ERRNO_BADF;if(n<5)return __WASI_ERRNO_NAMETOOLONG;memcpy(b,"/data",5);return 0;}
__wasi_errno_t __wasi_path_open(__wasi_fd_t fd,__wasi_lookupflags_t lookup,const char *name,__wasi_oflags_t of,__wasi_rights_t rights,__wasi_rights_t inherit,__wasi_fdflags_t fl,__wasi_fd_t *out){char path[512];if(!pathname(fd,name,path))return __WASI_ERRNO_NOTCAPABLE;int slot=4;while(slot<FDCAP&&fds[slot].used)slot++;if(slot==FDCAP)return __WASI_ERRNO_MFILE;FD *f=&fds[slot];int rc;
 if(of&__WASI_OFLAGS_DIRECTORY){rc=aster_fs_opendir(path,strlen(path),&f->h);f->dir=1;}else{uint32_t flags=(rights&__WASI_RIGHTS_FD_WRITE)?((rights&__WASI_RIGHTS_FD_READ)?ASTER_FS_O_RDWR:ASTER_FS_O_WRONLY):ASTER_FS_O_RDONLY;if(of&__WASI_OFLAGS_CREAT)flags|=ASTER_FS_O_CREAT;if(of&__WASI_OFLAGS_TRUNC)flags|=ASTER_FS_O_TRUNC;if(fl&__WASI_FDFLAGS_APPEND)flags|=ASTER_FS_O_APPEND;rc=aster_fs_open(path,strlen(path),flags,&f->h);f->dir=0;}
 if(rc<0)return err(rc);f->used=1;f->pos=f->cookie=0;f->hasPending=0;strcpy(f->path,path);*out=slot;return 0;}
__wasi_errno_t __wasi_fd_close(__wasi_fd_t fd){if(!valid(fd)||fd==3)return __WASI_ERRNO_BADF;int rc=fds[fd].dir?aster_fs_closedir(fds[fd].h):aster_fs_close(fds[fd].h);fds[fd].used=0;return err(rc);}
__wasi_errno_t __wasi_fd_read(__wasi_fd_t fd,const __wasi_iovec_t *v,size_t n,__wasi_size_t *out){*out=0;if(!valid(fd)||fd==3)return __WASI_ERRNO_BADF;for(size_t i=0;i<n;i++){int rc=aster_fs_read(fds[fd].h,v[i].buf,v[i].buf_len);if(rc<0)return *out?0:err(rc);*out+=rc;fds[fd].pos+=rc;if(rc<(int)v[i].buf_len)break;}return 0;}
__wasi_errno_t __wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *v,size_t n,__wasi_size_t *out){*out=0;for(size_t i=0;i<n;i++){if(fd==1||fd==2){char line[256];size_t at=0;while(at<v[i].buf_len){size_t len=v[i].buf_len-at;if(len>255)len=255;memcpy(line,(const char*)v[i].buf+at,len);line[len]=0;aster_printf("%s",line);at+=len;}*out+=v[i].buf_len;}else{if(!valid(fd)||fd==3)return __WASI_ERRNO_BADF;int rc=aster_fs_write(fds[fd].h,v[i].buf,v[i].buf_len);if(rc<0)return *out?0:err(rc);*out+=rc;fds[fd].pos+=rc;if(rc<(int)v[i].buf_len)break;}}return 0;}
static __wasi_errno_t stat_path(const char *name,__wasi_filestat_t *s){aster_stat_t a={.size=sizeof(a),.version=1};int rc=aster_fs_stat(name,strlen(name),&a,sizeof(a));if(rc<0)return err(rc);memset(s,0,sizeof(*s));s->filetype=(a.mode&ASTER_STAT_DIR)?__WASI_FILETYPE_DIRECTORY:__WASI_FILETYPE_REGULAR_FILE;s->size=a.filesize;s->nlink=1;return 0;}
__wasi_errno_t __wasi_path_filestat_get(__wasi_fd_t fd,__wasi_lookupflags_t flags,const char *name,__wasi_filestat_t *s){char path[512];if(!pathname(fd,name,path))return __WASI_ERRNO_NOTCAPABLE;return stat_path(path,s);}
__wasi_errno_t __wasi_fd_filestat_get(__wasi_fd_t fd,__wasi_filestat_t *s){if(!valid(fd))return __WASI_ERRNO_BADF;return stat_path(fd==3?"/data":fds[fd].path,s);}
__wasi_errno_t __wasi_fd_fdstat_get(__wasi_fd_t fd,__wasi_fdstat_t *s){memset(s,0,sizeof(*s));if(fd<3){s->fs_filetype=__WASI_FILETYPE_CHARACTER_DEVICE;s->fs_rights_base=__WASI_RIGHTS_FD_READ|__WASI_RIGHTS_FD_WRITE;return 0;}if(!valid(fd))return __WASI_ERRNO_BADF;s->fs_filetype=(fd==3||fds[fd].dir)?__WASI_FILETYPE_DIRECTORY:__WASI_FILETYPE_REGULAR_FILE;s->fs_rights_base=s->fs_rights_inheriting=~0ULL;return 0;}
__wasi_errno_t __wasi_fd_fdstat_set_flags(__wasi_fd_t fd,__wasi_fdflags_t flags){return valid(fd)?0:__WASI_ERRNO_BADF;}
__wasi_errno_t __wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t offset,__wasi_whence_t whence,__wasi_filesize_t *out){if(!valid(fd)||fd==3||fds[fd].dir)return __WASI_ERRNO_BADF;if(offset==0&&whence==__WASI_WHENCE_CUR){*out=fds[fd].pos;return 0;}int64_t target=offset;if(whence==__WASI_WHENCE_CUR)target+=fds[fd].pos;else if(whence==__WASI_WHENCE_END){__wasi_filestat_t s;int rc=stat_path(fds[fd].path,&s);if(rc)return rc;target+=s.size;}if(target<0||target>0x7fffffff)return __WASI_ERRNO_INVAL;int rc=aster_fs_seek(fds[fd].h,target,ASTER_FS_SEEK_SET);if(rc<0)return err(rc);*out=fds[fd].pos=target;return 0;}
__wasi_errno_t __wasi_fd_tell(__wasi_fd_t fd,__wasi_filesize_t *out){if(!valid(fd))return __WASI_ERRNO_BADF;*out=fds[fd].pos;return 0;}
__wasi_errno_t __wasi_fd_readdir(__wasi_fd_t fd,uint8_t *buf,__wasi_size_t len,__wasi_dircookie_t cookie,__wasi_size_t *out){
 *out=0;if(!valid(fd)||fd==3||!fds[fd].dir)return __WASI_ERRNO_BADF;FD *f=&fds[fd];
 if(cookie!=f->cookie){aster_fs_closedir(f->h);int rc=aster_fs_opendir(f->path,strlen(f->path),&f->h);if(rc<0)return err(rc);f->cookie=0;f->hasPending=0;}
 char skip[256];uint32_t dir;while(f->cookie<cookie){int rc=aster_fs_dir_next(f->h,skip,sizeof(skip),&dir);if(rc<=0)return rc<0?err(rc):0;f->cookie++;}
 while(*out<len){if(!f->hasPending){int rc=aster_fs_dir_next(f->h,f->pending,sizeof(f->pending),&f->pendingDir);if(rc<=0)return rc<0?err(rc):0;f->hasPending=1;}
  __wasi_dirent_t e={0};e.d_next=f->cookie+1;e.d_namlen=strlen(f->pending);e.d_type=f->pendingDir?__WASI_FILETYPE_DIRECTORY:__WASI_FILETYPE_REGULAR_FILE;
  uint32_t start=*out,space=len-start,total=sizeof(e)+e.d_namlen,take=space<sizeof(e)?space:sizeof(e);memcpy(buf+*out,&e,take);*out+=take;space-=take;
  take=space<e.d_namlen?space:e.d_namlen;memcpy(buf+*out,f->pending,take);*out+=take;
  if(total>len-start)return 0;
  if(take==e.d_namlen){f->cookie++;f->hasPending=0;}
 }
 return 0;
}
__wasi_errno_t __wasi_path_create_directory(__wasi_fd_t fd,const char *name){char path[512];if(!pathname(fd,name,path))return __WASI_ERRNO_NOTCAPABLE;if(!strcmp(path,"/data")||!strcmp(path,"/data/")||!strcmp(path,"/data/."))return __WASI_ERRNO_EXIST;return err(aster_fs_mkdir(path,strlen(path)));}
__wasi_errno_t __wasi_path_unlink_file(__wasi_fd_t fd,const char *name){char path[512];if(!pathname(fd,name,path))return __WASI_ERRNO_NOTCAPABLE;return err(aster_fs_unlink(path,strlen(path)));}
__wasi_errno_t __wasi_fd_sync(__wasi_fd_t fd){return valid(fd)?0:__WASI_ERRNO_BADF;}
__wasi_errno_t __wasi_fd_datasync(__wasi_fd_t fd){return __wasi_fd_sync(fd);}
__wasi_errno_t __wasi_args_sizes_get(size_t *count,size_t *bytes){*count=0;*bytes=0;return 0;}
__wasi_errno_t __wasi_args_get(uint8_t **args,uint8_t *buf){return 0;}
__wasi_errno_t __wasi_environ_sizes_get(size_t *count,size_t *bytes){*count=0;*bytes=0;return 0;}
__wasi_errno_t __wasi_environ_get(uint8_t **env,uint8_t *buf){return 0;}
__wasi_errno_t __wasi_clock_res_get(__wasi_clockid_t id,__wasi_timestamp_t *ns){*ns=1000000;return 0;}
__wasi_errno_t __wasi_clock_time_get(__wasi_clockid_t id,__wasi_timestamp_t precision,__wasi_timestamp_t *ns){uint64_t ms=aster_time_now_ms();if(id==__WASI_CLOCKID_REALTIME){aster_time_info_t t={.size=sizeof(t),.version=1};if(aster_time_get(&t,sizeof(t))>=0)ms=t.unix_ms;}*ns=ms*1000000ULL;return 0;}
__wasi_errno_t __wasi_sched_yield(void){return 0;}
__attribute__((noreturn)) void __wasi_proc_exit(__wasi_exitcode_t status){tyrian_exit(status);}
char *mkdtemp(char *s){static unsigned id;snprintf(s,20,"/data/tmp%u",++id);return aster_fs_mkdir(s,strlen(s))<0?NULL:s;}

__wasi_errno_t __wasi_path_remove_directory(__wasi_fd_t fd,const char *name){return __wasi_path_unlink_file(fd,name);}
