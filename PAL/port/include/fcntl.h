#include "pal_libc.h"
#define O_RDONLY 0
int open(const char *,int,...);
int close(int);
ssize_t read(int,void *,size_t);
off_t lseek(int,off_t,int);
