#ifndef PAL_ASTER_LIBC_H
#define PAL_ASTER_LIBC_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PAL_FILE FILE;
typedef long time_t;
struct tm { int tm_sec,tm_min,tm_hour,tm_mday,tm_mon,tm_year,tm_wday,tm_yday,tm_isdst; };
struct timeval { long tv_sec, tv_usec; };
typedef long ssize_t;
typedef long off_t;
extern FILE *stdin, *stdout, *stderr;
extern int errno;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define ENOENT 2
#define EIO 5
#define EINVAL 22
#define M_PI 3.14159265358979323846
FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fread(void *,size_t,size_t,FILE *);
size_t fwrite(const void *,size_t,size_t,FILE *);
int fseek(FILE *,long,int);
long ftell(FILE *);
char *fgets(char *,int,FILE *);
int fputs(const char *,FILE *);
int fgetc(FILE *);
int fputc(int,FILE *);
int feof(FILE *);
int ferror(FILE *);
int fflush(FILE *);
void rewind(FILE *);
int remove(const char *);
int printf(const char *,...);
int fprintf(FILE *,const char *,...);
int vfprintf(FILE *,const char *,va_list);
int snprintf(char *,size_t,const char *,...);
int vsnprintf(char *,size_t,const char *,va_list);
int sprintf(char *,const char *,...);
int sscanf(const char *,const char *,...);
void *memcpy(void *,const void *,size_t);
void *memmove(void *,const void *,size_t);
void *memset(void *,int,size_t);
int memcmp(const void *,const void *,size_t);
size_t strlen(const char *);
size_t strnlen(const char *,size_t);
char *strcpy(char *,const char *);
char *strncpy(char *,const char *,size_t);
char *strcat(char *,const char *);
char *strncat(char *,const char *,size_t);
int strcmp(const char *,const char *);
int strncmp(const char *,const char *,size_t);
char *strchr(const char *,int);
char *strrchr(const char *,int);
char *strstr(const char *,const char *);
int strcasecmp(const char *,const char *);
int strncasecmp(const char *,const char *,size_t);
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
int atoi(const char *);
long strtol(const char *,char **,int);
unsigned long strtoul(const char *,char **,int);
double atof(const char *);
int abs(int);
int rand(void);
void srand(unsigned int);
void qsort(void *,size_t,size_t,int (*)(const void *,const void *));
char *getenv(const char *);
int access(const char *,int);
int unlink(const char *);
time_t time(time_t *);
struct tm *localtime(const time_t *);
int gettimeofday(struct timeval *,void *);
int tolower(int);
int toupper(int);
int isspace(int);
int isdigit(int);
int isalpha(int);
int isalnum(int);
int isprint(int);
double fmax(double,double);
double fmin(double,double);
double fabs(double);
double floor(double);
double ceil(double);
double round(double);
double pow(double,double);
double sqrt(double);
double sin(double);
double cos(double);
double tan(double);
double atan2(double,double);
void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
void pal_assert(const char *,const char *,int) __attribute__((noreturn));
#define assert(x) ((x)?(void)0:pal_assert(#x,__FILE__,__LINE__))
typedef int jmp_buf[1];
int setjmp(jmp_buf);
void longjmp(jmp_buf,int) __attribute__((noreturn));
size_t wcslen(const wchar_t *);
wchar_t *wcscpy(wchar_t *,const wchar_t *);
int wcscmp(const wchar_t *,const wchar_t *);
wchar_t *wcschr(const wchar_t *,wchar_t);
long wcstol(const wchar_t *,wchar_t **,int);
int vswprintf(wchar_t *,size_t,const wchar_t *,va_list);
#ifdef __cplusplus
}
#endif
#endif
