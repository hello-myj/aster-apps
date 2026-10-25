#include <aster_imports.h>
#include "utils/log.h"
#include "utils/crash.h"
#include "utils/msgbox.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static int level=LOG_INFO;static char lastError[512];
void log_init(void){lastError[0]=0;}void log_close(void){}
void log_set_level(log_level l){level=l;}void log_set_colors(bool c){}
void log_add_stderr(log_level l,bool c){}void log_add_file(const char *n,log_level l){}
log_level log_level_text_to_enum(const char *s,log_level d){const char *n[]={"DEBUG","INFO","WARN","ERROR"};for(int i=0;i<4;i++)if(!strcmp(n[i],s))return i;return d;}
bool is_log_level(const char *s){return log_level_text_to_enum(s,99)!=99;}
void log_msg(log_level l,const char *fmt,...){if((int)l<level)return;char b[768];va_list v;va_start(v,fmt);vsnprintf(b,sizeof(b),fmt,v);va_end(v);if(l==LOG_ERROR)snprintf(lastError,sizeof(lastError),"%s",b);aster_printf("OMF[%d] %s",l,b);}
const char *log_last_error(void){return lastError;}
_Noreturn void _crash(const char *fn,const char *file,int line,const char *fmt,...){char b[768];va_list v;va_start(v,fmt);vsnprintf(b,sizeof(b),fmt,v);va_end(v);aster_printf("OMF FATAL %s:%d %s: %s",file,line,fn,b);__builtin_trap();}
void err_msgbox(const char *fmt,...){char b[512];va_list v;va_start(v,fmt);vsnprintf(b,sizeof(b),fmt,v);va_end(v);aster_printf("OMF ERROR %s",b);}
