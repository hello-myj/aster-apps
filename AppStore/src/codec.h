/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {int start,end,next,count;char type;} json_token;
typedef struct {const char *text;json_token tokens[768];int used;} json_doc;
int json_parse(json_doc *d,const char *text,int length);
int json_key(const json_doc *d,int object,const char *key);
int json_string(const json_doc *d,int token,char *out,size_t size);
int json_uint(const json_doc *d,int token,uint32_t *out);
typedef struct {uint32_t h[8];uint64_t bytes;uint8_t block[64];unsigned used;} store_sha256;
void sha_init(store_sha256 *s);
void sha_update(store_sha256 *s,const void *data,size_t n);
void sha_hex(store_sha256 *s,char output[65]);
