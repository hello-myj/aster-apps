#include "pal_libc.h"
typedef struct PAL_DIR DIR;
struct dirent { char d_name[256]; unsigned char d_type; };
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
