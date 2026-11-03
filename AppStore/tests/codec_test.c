#include <assert.h>
#include <string.h>
#include "../src/codec.h"
int main(void)
{
    store_sha256 s;char hash[65];sha_init(&s);sha_hex(&s,hash);
    assert(!strcmp(hash,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha_init(&s);sha_update(&s,"a",1);sha_update(&s,"bc",2);sha_hex(&s,hash);
    assert(!strcmp(hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    sha_init(&s);for(int i=0;i<1000000;i++)sha_update(&s,"a",1);sha_hex(&s,hash);
    assert(!strcmp(hash,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    static json_doc d;char text[32];uint32_t n;
    const char *json="{\"items\":[{\"name\":\"商城\",\"bytes\":4294967295}],\"next\":-1}";
    assert(!json_parse(&d,json,strlen(json)));int a=json_key(&d,0,"items");assert(a>0 && d.tokens[a].count==1);
    assert(json_string(&d,json_key(&d,a+1,"name"),text,sizeof(text))>0 && !strcmp(text,"商城"));
    assert(!json_uint(&d,json_key(&d,a+1,"bytes"),&n) && n==UINT32_MAX);
    const char *bad[]={"", "{", "[1,]", "{\"a\":}", "[01]", "[4294967296]", "\"bad\nline\"", "{} trailing"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        int r=json_parse(&d,bad[i],strlen(bad[i]));
        if(i==5){assert(!r && json_uint(&d,1,&n)<0);}else assert(r<0);
    }
}
