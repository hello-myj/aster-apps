#include "pal_libc.h"
#include <aster_imports.h>
size_t strlen(const char *s){size_t n=0;while(s[n])n++;return n;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char *a,const char *b,size_t n){while(n--){int x=(uint8_t)*a++,y=(uint8_t)*b++;if(x!=y||!x)return x-y;}return 0;}
char *strncpy(char *d,const char *s,size_t n){char *p=d;while(n--){if(*s)*d++=*s++;else *d++=0;}return p;}
char *strchr(const char *s,int c){do{if(*s==c)return (char*)s;}while(*s++);return NULL;}
size_t strnlen(const char *s,size_t n){size_t i=0;while(i<n&&s[i])i++;return i;}
char *strcpy(char *d,const char *s){char *r=d;while((*d++=*s++));return r;}
char *strcat(char *d,const char *s){strcpy(d+strlen(d),s);return d;}
char *strncat(char *d,const char *s,size_t n){char *p=d+strlen(d);while(n--&&*s)*p++=*s++;*p=0;return d;}
char *strrchr(const char *s,int c){char *p=NULL;do{if(*s==c)p=(char*)s;}while(*s++);return p;}
char *strstr(const char *s,const char *p){size_t n=strlen(p);for(;*s;s++)if(!strncmp(s,p,n))return (char*)s;return n?NULL:(char*)s;}
int tolower(int c){return c>='A'&&c<='Z'?c+32:c;}int toupper(int c){return c>='a'&&c<='z'?c-32:c;}
int isspace(int c){return c==' '||(c>=9&&c<=13);}int isdigit(int c){return c>='0'&&c<='9';}int isalpha(int c){c=tolower(c);return c>='a'&&c<='z';}int isalnum(int c){return isalpha(c)||isdigit(c);}int isprint(int c){return c>=32&&c<127;}
int strncasecmp(const char *a,const char *b,size_t n){while(n--){int x=tolower((uint8_t)*a++),y=tolower((uint8_t)*b++);if(x!=y||!x)return x-y;}return 0;}
int strcasecmp(const char *a,const char *b){return strncasecmp(a,b,(size_t)-1);}
long strtol(const char *s,char **end,int base){while(isspace(*s))s++;int sign=1;if(*s=='-'){sign=-1;s++;}else if(*s=='+')s++;if(!base)base=10;long v=0;while(*s){int d=isdigit(*s)?*s-'0':tolower(*s)-'a'+10;if(d<0||d>=base)break;v=v*base+d;s++;}if(end)*end=(char*)s;return v*sign;}
unsigned long strtoul(const char *s,char **p,int b){return (unsigned long)strtol(s,p,b);}int atoi(const char *s){return (int)strtol(s,NULL,10);}double atof(const char *s){return (double)strtol(s,NULL,10);}
int abs(int x){return x<0?-x:x;}double fabs(double x){return x<0?-x:x;}double fmin(double x,double y){return x<y?x:y;}double fmax(double x,double y){return x>y?x:y;}
double floor(double x){int64_t n=(int64_t)x;return x<n?n-1:n;}double ceil(double x){return -floor(-x);}double round(double x){return x<0?-floor(-x+0.5):floor(x+0.5);}
char *getenv(const char *p){(void)p;return NULL;}
static uint32_t seed=1;void srand(unsigned n){seed=n;}int rand(void){seed=seed*1664525+1013904223;return (int)(seed>>1);}
time_t time(time_t *p){aster_time_info_t t={.size=sizeof(t),.version=1};time_t v=aster_time_get(&t,sizeof(t))<0?0:(time_t)(t.unix_ms/1000);if(p)*p=v;return v;}
struct tm *localtime(const time_t *p){(void)p;static struct tm out;aster_time_info_t t={.size=sizeof(t),.version=1};if(!aster_time_get(&t,sizeof(t))){out.tm_year=t.year-1900;out.tm_mon=t.month-1;out.tm_mday=t.day;out.tm_hour=t.hour;out.tm_min=t.minute;out.tm_sec=t.second;}return &out;}
int gettimeofday(struct timeval *p,void *tz){(void)tz;uint32_t n=aster_time_now_ms();p->tv_sec=n/1000;p->tv_usec=(n%1000)*1000;return 0;}
size_t wcslen(const wchar_t *s){size_t n=0;while(s[n])n++;return n;}wchar_t *wcscpy(wchar_t *d,const wchar_t *s){wchar_t *p=d;while((*d++=*s++));return p;}
int wcscmp(const wchar_t *a,const wchar_t *b){while(*a&&*a==*b){a++;b++;}return *a-*b;}
wchar_t *wcschr(const wchar_t *s,wchar_t c){do{if(*s==c)return (wchar_t*)s;}while(*s++);return NULL;}
long wcstol(const wchar_t *s,wchar_t **end,int base){char tmp[64];size_t n=0;while(s[n]&&n<63){tmp[n]=(char)s[n];n++;}tmp[n]=0;char *e;long v=strtol(tmp,&e,base);if(end)*end=(wchar_t*)s+(e-tmp);return v;}
int vsnprintf(char *out,size_t cap,const char *fmt,va_list ap){size_t n=0;
#define PUT(c) do{if(cap&&n<cap-1)out[n]=(c);n++;}while(0)
while(*fmt){if(*fmt!='%'){PUT(*fmt++);continue;}fmt++;if(*fmt=='%'){PUT(*fmt++);continue;}while(*fmt=='-'||*fmt=='+'||*fmt=='0'||*fmt==' '||*fmt=='#')fmt++;
while(isdigit(*fmt))fmt++;if(*fmt=='*'){(void)va_arg(ap,int);fmt++;}int precision=-1;if(*fmt=='.'){fmt++;if(*fmt=='*'){precision=va_arg(ap,int);fmt++;}else{precision=0;while(isdigit(*fmt))precision=precision*10+(*fmt++-'0');}}
int length=0;while(*fmt=='l'||*fmt=='z'||*fmt=='h'){if(*fmt=='l')length++;fmt++;}char c=*fmt?*fmt++:0;
if(c=='s'){const char *s=va_arg(ap,const char*);if(!s)s="(null)";for(int i=0;*s&&(precision<0||i<precision);i++)PUT(*s++);}
else if(c=='c'){PUT(va_arg(ap,int));}
else if(c=='d'||c=='i'||c=='u'||c=='x'||c=='X'||c=='p'){uint64_t v;if(c=='d'||c=='i'){int64_t s=length>1?va_arg(ap,long long):va_arg(ap,int);if(s<0){PUT('-');v=(uint64_t)(-s);}else v=s;}else v=length>1?va_arg(ap,unsigned long long):va_arg(ap,unsigned int);unsigned base=(c=='x'||c=='X'||c=='p')?16:10;char b[32];int k=0;do{b[k++]="0123456789abcdef"[v%base];v/=base;}while(v&&k<32);while(k)PUT(b[--k]);}
else if(c=='f'||c=='g'){(void)va_arg(ap,double);PUT('?');}
}if(cap)out[n<cap?n:cap-1]=0;return (int)n;
#undef PUT
}
int snprintf(char *d,size_t n,const char *fmt,...){va_list a;va_start(a,fmt);int r=vsnprintf(d,n,fmt,a);va_end(a);return r;}
int sprintf(char *d,const char *fmt,...){va_list a;va_start(a,fmt);int r=vsnprintf(d,1024,fmt,a);va_end(a);return r;}
int vfprintf(FILE *f,const char *fmt,va_list a){(void)f;char s[1024];int n=vsnprintf(s,sizeof(s),fmt,a);aster_log(ASTER_LOG_INFO,s,(uint32_t)strnlen(s,sizeof(s)));return n;}
int fprintf(FILE *f,const char *fmt,...){va_list a;va_start(a,fmt);int r=vfprintf(f,fmt,a);va_end(a);return r;}int printf(const char *fmt,...){va_list a;va_start(a,fmt);int r=vfprintf(NULL,fmt,a);va_end(a);return r;}
int vswprintf(wchar_t *out,size_t cap,const wchar_t *fmt,va_list a){char f[256],o[512];size_t i=0;while(fmt[i]&&i<255){f[i]=(char)fmt[i];i++;}f[i]=0;int n=vsnprintf(o,sizeof(o),f,a);size_t k=0;while(o[k]&&k+1<cap){out[k]=(uint8_t)o[k];k++;}if(cap)out[k]=0;return n;}
extern void PAL_AsterQuit(int);
void exit(int c){PAL_AsterQuit(c);__builtin_unreachable();}void abort(void){aster_errfmt("PAL abort");exit(1);}void pal_assert(const char *x,const char *file,int line){aster_errfmt("PAL assert %s at %s:%d",x,file,line);exit(1);}
