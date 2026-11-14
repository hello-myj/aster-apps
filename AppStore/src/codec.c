/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "codec.h"
/* Freestanding; declarations match the SDK and the native test libc. */
void *memcpy(void *,const void *,size_t);
void *memset(void *,int,size_t);
size_t strlen(const char *);
int memcmp(const void *,const void *,size_t);
static int white(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
static int token(json_doc *d,int *p,int len,int depth)
{
    while(*p<len && white(d->text[*p]))(*p)++;
    if(*p>=len || depth>12 || d->used>=768)return -1;
    int id=d->used++;json_token *t=&d->tokens[id];
    *t=(json_token){.start=*p,.type=d->text[*p]};char c=t->type;(*p)++;
    if(c=='{' || c=='[') {
        char close=c=='{'?'}':']';
        while(*p<len && white(d->text[*p]))(*p)++;
        if(*p<len && d->text[*p]==close){(*p)++;goto done;}
        for(;;) {
            if(c=='{') {
                int k=token(d,p,len,depth+1);if(k<0 || d->tokens[k].type!='"')return -1;
                while(*p<len && white(d->text[*p]))(*p)++;
                if(*p>=len || d->text[(*p)++]!=':')return -1;
            }
            if(token(d,p,len,depth+1)<0)return -1;
            t->count++;
            while(*p<len && white(d->text[*p]))(*p)++;
            if(*p>=len)return -1;
            char sep=d->text[(*p)++];if(sep==close)break;if(sep!=',')return -1;
        }
    } else if(c=='"') {
        int closed=0;
        while(*p<len) {
            unsigned char ch=d->text[(*p)++];if(ch=='"'){closed=1;break;}
            if(ch<32)return -1;
            if(ch=='\\') {
                if(*p>=len)return -1;
                char esc=d->text[(*p)++];
                if(esc=='u') {for(int i=0;i<4;i++){if(*p>=len)return -1;char h=d->text[(*p)++];if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F')))return -1;}}
                else if(esc!='"'&&esc!='\\'&&esc!='/'&&esc!='b'&&esc!='f'&&esc!='n'&&esc!='r'&&esc!='t')return -1;
            }
        }
        if(!closed)return -1;
    } else {
        while(*p<len && !white(d->text[*p]) && d->text[*p]!=',' && d->text[*p]!=']' && d->text[*p]!='}')(*p)++;
        int n=*p-t->start;const char *s=d->text+t->start;
        if(!((n==4 && !memcmp(s,"true",4)) || (n==5 && !memcmp(s,"false",5)) ||
             (n==4 && !memcmp(s,"null",4)))) {
            int i=s[0]=='-'?1:0;if(i==n)return -1;
            if(n-i>1 && s[i]=='0')return -1;
            for(;i<n;i++)if(s[i]<'0'||s[i]>'9')return -1;
        }
    }
done:t->end=*p;t->next=d->used;return id;
}
int json_parse(json_doc *d,const char *text,int length)
{
    d->text=text;d->used=0;int p=0;
    if(token(d,&p,length,0)!=0)return -1;
    while(p<length && white(text[p]))p++;
    return p==length?0:-1;
}
int json_string(const json_doc *d,int id,char *out,size_t size)
{
    if(id<0 || id>=d->used || d->tokens[id].type!='"' || !size)return -1;
    const json_token *t=&d->tokens[id];size_t n=0;
    for(int i=t->start+1;i<t->end-1;i++) {
        char c=d->text[i];
        if(c=='\\') {c=d->text[++i];if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else if(c=='u'||c=='b'||c=='f')return -1;}
        if(!c || n+1>=size)return -1;
        out[n++]=c;
    }
    out[n]=0;return (int)n;
}
int json_key(const json_doc *d,int object,const char *name)
{
    if(object<0 || object>=d->used || d->tokens[object].type!='{')return -1;
    int k=object+1;
    for(int i=0;i<d->tokens[object].count;i++) {
        char key[48];int value=d->tokens[k].next;
        if(json_string(d,k,key,sizeof(key))>=0 && strlen(name)==strlen(key) && !memcmp(key,name,strlen(name)))return value;
        k=d->tokens[value].next;
    }
    return -1;
}
int json_uint(const json_doc *d,int id,uint32_t *out)
{
    if(id<0 || id>=d->used)return -1;
    json_token t=d->tokens[id];uint32_t n=0;
    if(t.type<'0'||t.type>'9')return -1;
    for(int p=t.start;p<t.end;p++) {unsigned digit=(unsigned)(d->text[p]-'0');if(digit>9 || n>(UINT32_MAX-digit)/10)return -1;n=n*10+digit;}
    *out=n;return 0;
}
static uint32_t rotr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static const uint32_t constants[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static void block(store_sha256 *s)
{
    uint32_t w[64];for(unsigned i=0;i<16;i++)w[i]=((uint32_t)s->block[i*4]<<24)|((uint32_t)s->block[i*4+1]<<16)|((uint32_t)s->block[i*4+2]<<8)|s->block[i*4+3];
    for(unsigned i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));}
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;i++){uint32_t t=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+constants[i]+w[i];uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
void sha_init(store_sha256 *s)
{*s=(store_sha256){.h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};}
void sha_update(store_sha256 *s,const void *data,size_t n)
{
    const uint8_t *p=data;s->bytes+=n;
    while(n){size_t take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,p,take);p+=take;n-=take;s->used+=(unsigned)take;if(s->used==64){block(s);s->used=0;}}
}
void sha_hex(store_sha256 *s,char output[65])
{
    uint64_t bits=s->bytes*8;uint8_t end=0x80,zero=0;sha_update(s,&end,1);
    while(s->used!=56)sha_update(s,&zero,1);
    uint8_t length[8];for(unsigned i=0;i<8;i++)length[7-i]=(uint8_t)(bits>>(i*8));sha_update(s,length,8);
    const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<32;i++){unsigned value=(s->h[i/4]>>(24-8*(i%4)))&255;output[2*i]=hex[value>>4];output[2*i+1]=hex[value&15];}output[64]=0;
}

static int version_parts(const char *s,unsigned parts[3])
{
    if(!s)return -1;
    for(unsigned i=0;i<3;i++){
        if(*s<'0'||*s>'9')return -1;
        unsigned n=0;do{n=n*10+(unsigned)(*s++-'0');if(n>255)return -1;}while(*s>='0'&&*s<='9');parts[i]=n;
        if(!*s){if(!i)return -1;if(i==1)parts[2]=0;return 0;}
        if(*s++!='.'||i==2)return -1;
    }
    return -1;
}
int store_version_compare(const char *a,const char *b,int *order)
{
    unsigned av[3],bv[3];if(!order||version_parts(a,av)||version_parts(b,bv))return -1;
    *order=0;for(unsigned i=0;i<3;i++)if(av[i]!=bv[i]){*order=av[i]>bv[i]?1:-1;break;}return 0;
}

static char *decimal(char *p,uint32_t value)
{
    char digits[10];unsigned n=0;
    do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
    while(n)*p++=digits[--n];
    return p;
}
void store_format_bytes(uint32_t bytes,char output[32])
{
    uint32_t divisor=bytes>=1048576?1048576:bytes>=1024?1024:1;
    char *p=decimal(output,bytes/divisor);
    if(divisor>1){*p++='.';*p++=(char)('0'+((uint64_t)(bytes%divisor)*10/divisor));}
    *p++=' ';if(divisor>1)*p++=divisor==1024?'K':'M';*p++='B';*p=0;
}
void store_rate_reset(store_rate *rate,uint32_t now,uint32_t bytes)
{*rate=(store_rate){.sampled_ms=now,.sampled_bytes=bytes};}
int store_rate_sample(store_rate *rate,uint32_t now,uint32_t bytes)
{
    uint32_t elapsed=now-rate->sampled_ms;
    if(bytes<rate->sampled_bytes){store_rate_reset(rate,now,bytes);return 1;}
    if(elapsed<1000)return 0;
    uint64_t speed=(uint64_t)(bytes-rate->sampled_bytes)*1000/elapsed;
    rate->bytes_per_second=speed>UINT32_MAX?UINT32_MAX:(uint32_t)speed;
    rate->sampled_ms=now;rate->sampled_bytes=bytes;return 1;
}
