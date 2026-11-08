#include "pal_libc.h"
struct stat { long st_size; unsigned st_mode; };
#define S_IFDIR 0040000
#define S_ISDIR(x) ((x)&S_IFDIR)
int stat(const char *,struct stat *);
int fstat(int,struct stat *);
int mkdir(const char *,int);
