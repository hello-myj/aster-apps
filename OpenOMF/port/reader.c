#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/internal/reader.h"
#include "utils/allocator.h"
#include "utils/log.h"
#include "utils/c_string_util.h"

struct sd_reader {
    FILE *handle;
    long filesize;
    int std_errno;
    long pos, cache_start;
    size_t cache_len;
    int eof;
    unsigned char cache[8192];
};

sd_reader *sd_reader_open(const path *filename) {
    sd_reader *reader = omf_calloc(1, sizeof(sd_reader));
    reader->std_errno = 0;
    // Attempt to open file (note: Binary mode!)
    reader->handle = path_fopen(filename, "rb");
    if(!reader->handle) {
        log_error("C reader fopen failed %s errno=%d (%s)",path_c(filename),errno,strerror(errno));
        omf_free(reader);
        return NULL;
    }

    // Find file size
    if(fseek(reader->handle, 0, SEEK_END) == -1) {
        log_error("C reader seek-end failed %s errno=%d (%s)",path_c(filename),errno,strerror(errno));
        goto error;
    }
    reader->filesize = ftell(reader->handle);
    if(reader->filesize == -1) {
        log_error("C reader tell failed %s errno=%d (%s)",path_c(filename),errno,strerror(errno));
        goto error;
    }
    if(fseek(reader->handle, 0, SEEK_SET) == -1) {
        log_error("C reader seek-start failed %s errno=%d (%s)",path_c(filename),errno,strerror(errno));
        goto error;
    }

    /* Binary parsers seek over short fields thousands of times. Keep their
     * logical cursor in this bounded cache instead of issuing an SD seek for
     * every field; memory use stays independent of resource size. */
    setvbuf(reader->handle, NULL, _IONBF, 0);
    reader->cache_start = -1;
    return reader;

error:
    fclose(reader->handle);
    omf_free(reader);
    return NULL;
}

long sd_reader_filesize(const sd_reader *reader) {
    return reader->filesize;
}

int sd_reader_errno(const sd_reader *reader) {
    return reader->std_errno;
}

void sd_reader_close(sd_reader *reader) {
    fclose(reader->handle);
    omf_free(reader);
}

int sd_reader_set(sd_reader *reader, long offset) {
    if(offset < 0) { reader->std_errno = EINVAL; return 0; }
    reader->pos = offset; reader->eof = 0; return 1;
}
int sd_reader_ok(const sd_reader *reader) { return !reader->eof; }
long sd_reader_pos(sd_reader *reader) { return reader->pos; }
int sd_read_buf(sd_reader *reader, char *buf, size_t len) {
    while(len) {
        if(reader->pos >= reader->filesize) { reader->eof=1; return 0; }
        long at=reader->pos-reader->cache_start;
        if(reader->cache_start<0 || at<0 || (size_t)at>=reader->cache_len) {
            if(fseek(reader->handle,reader->pos,SEEK_SET)) { reader->std_errno=errno;return 0; }
            reader->cache_start=reader->pos;
            reader->cache_len=fread(reader->cache,1,sizeof(reader->cache),reader->handle);
            at=0;
            if(!reader->cache_len) {reader->std_errno=ferror(reader->handle);reader->eof=1;return 0;}
        }
        size_t n=reader->cache_len-(size_t)at;if(n>len)n=len;
        memcpy(buf,reader->cache+at,n);buf+=n;len-=n;reader->pos+=n;
    }
    return 1;
}
int sd_peek_buf(sd_reader *reader, char *buf, int len) {
    long pos=reader->pos;int eof=reader->eof;
    int ok=len>=0 && sd_read_buf(reader,buf,(size_t)len);
    reader->pos=pos;reader->eof=eof;return ok?0:1;
}

uint8_t sd_read_ubyte(sd_reader *reader) {
    uint8_t d = 0;
    sd_read_buf(reader, (char *)&d, 1);
    return d;
}

uint16_t sd_read_uword(sd_reader *reader) {
    uint16_t d = 0;
    sd_read_buf(reader, (char *)&d, 2);
    return d;
}

uint32_t sd_read_udword(sd_reader *reader) {
    uint32_t d = 0;
    sd_read_buf(reader, (char *)&d, 4);
    return d;
}

int8_t sd_read_byte(sd_reader *reader) {
    int8_t d = 0;
    sd_read_buf(reader, (char *)&d, 1);
    return d;
}

int16_t sd_read_word(sd_reader *reader) {
    int16_t d = 0;
    sd_read_buf(reader, (char *)&d, 2);
    return d;
}

int32_t sd_read_dword(sd_reader *reader) {
    int32_t d = 0;
    sd_read_buf(reader, (char *)&d, 4);
    return d;
}

float sd_read_float(sd_reader *reader) {
    float f = 0;
    sd_read_buf(reader, (char *)&f, 4);
    return f;
}

uint8_t sd_peek_ubyte(sd_reader *reader) {
    uint8_t d = 0;
    sd_peek_buf(reader, (char *)&d, 1);
    return d;
}

uint16_t sd_peek_uword(sd_reader *reader) {
    uint16_t d = 0;
    sd_peek_buf(reader, (char *)&d, 2);
    return d;
}

uint32_t sd_peek_udword(sd_reader *reader) {
    uint32_t d = 0;
    sd_peek_buf(reader, (char *)&d, 4);
    return d;
}

int8_t sd_peek_byte(sd_reader *reader) {
    int8_t d = 0;
    sd_peek_buf(reader, (char *)&d, 1);
    return d;
}

int16_t sd_peek_word(sd_reader *reader) {
    int16_t d = 0;
    sd_peek_buf(reader, (char *)&d, 2);
    return d;
}

int32_t sd_peek_dword(sd_reader *reader) {
    int32_t d = 0;
    sd_peek_buf(reader, (char *)&d, 4);
    return d;
}

float sd_peek_float(sd_reader *reader) {
    float f = 0;
    sd_peek_buf(reader, (char *)&f, 4);
    return f;
}

int sd_match(sd_reader *reader, const char *buf, unsigned int nbytes) {
    char *t = omf_malloc(nbytes);
    if(sd_peek_buf(reader, t, nbytes) == 0 && memcmp(t, buf, nbytes) == 0) {
        omf_free(t);
        return 1;
    }
    omf_free(t);
    return 0;
}

void sd_skip(sd_reader *reader, unsigned int nbytes) {
    sd_reader_set(reader,reader->pos+nbytes);
}
int sd_read_scan(const sd_reader *reader, const char *format, ...) {
    sd_reader *r=(sd_reader *)reader;
    if(fseek(r->handle,r->pos,SEEK_SET))return EOF;
    va_list args;va_start(args,format);int ret=vfscanf(r->handle,format,args);va_end(args);
    r->pos=ftell(r->handle);r->cache_start=-1;r->cache_len=0;r->eof=feof(r->handle);return ret;
}
int sd_read_line(const sd_reader *reader, char *buffer, int maxlen) {
    sd_reader *r=(sd_reader *)reader;int n=0;
    if(maxlen<=0)return 1;
    while(n<maxlen-1) {char c;if(!sd_read_buf(r,&c,1))break;buffer[n++]=c;if(c=='\n')break;}
    buffer[n]=0;return n?0:1;
}

bool sd_read_terminated_str(sd_reader *r, str *dst, uint16_t max_len) {
    const uint16_t len = sd_read_uword(r);
    if(len >= max_len) {
        return false;
    }
    // Read the string plus its trailing (uncounted) null byte.
    char *buf = omf_calloc(1, len + 2);
    sd_read_buf(r, buf, len + 1);
    const bool terminated = (buf[len] == 0);
    if(terminated) {
        str_from_c(dst, buf);
    }
    omf_free(buf);
    return terminated;
}

void sd_read_fixed_str(sd_reader *r, str *dst, size_t len) {
    char *buf = omf_calloc(1, len + 1); // Guaranteed null term at buf[len]
    sd_read_buf(r, buf, len);
    str_from_c(dst, buf);
    omf_free(buf);
}

char *sd_read_variable_str(sd_reader *r) {
    uint16_t len = sd_read_uword(r);
    char *str = NULL;
    if(len > 0) {
        str = (char *)omf_calloc(len, 1);
        sd_read_buf(r, str, len);
    }
    return str;
}

bool sd_read_padded_str(sd_reader *r, str *dst, uint16_t max_len) {
    const uint16_t len = sd_read_uword(r);
    if(len >= max_len) {
        return false;
    }
    if(len > 0) {
        char *buf = omf_calloc(1, len + 1);
        sd_read_buf(r, buf, len);
        str_from_c(dst, buf);
        omf_free(buf);
    } else {
        str_create(dst);
    }
    return true;
}
