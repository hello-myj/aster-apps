#include <assert.h>
#include <string.h>
#include "../src/codec.h"
int main(void)
{
    char size[32];store_format_bytes(1048576,size);assert(!strcmp(size,"1.0 MB"));
    store_format_bytes(1024,size);assert(!strcmp(size,"1.0 KB"));
    store_format_bytes(0,size);assert(!strcmp(size,"0 B"));
    store_format_bytes(UINT32_MAX,size);assert(!strcmp(size,"4095.9 MB"));
    store_rate rate;store_rate_reset(&rate,100,0);
    assert(!store_rate_sample(&rate,600,4096));
    assert(store_rate_sample(&rate,1100,65536)&&rate.bytes_per_second==65536);
    assert(store_rate_sample(&rate,3100,65536)&&!rate.bytes_per_second);
    store_rate_reset(&rate,UINT32_MAX-499,0);
    assert(store_rate_sample(&rate,500,1024)&&rate.bytes_per_second==1024);
    assert(store_rate_sample(&rate,1600,0)&&!rate.bytes_per_second);
    int order;
    assert(!store_version_compare("1.10.0","1.9.9",&order)&&order>0);
    assert(!store_version_compare("1.2","1.2.0",&order)&&!order);
    assert(!store_version_compare("2.0.0","10.0.0",&order)&&order<0);
    assert(!store_version_compare("0.9.255","0.10.0",&order)&&order<0);
    assert(!store_version_compare("01.02.003","1.2.3",&order)&&!order);
    const char *bad_versions[]={"","1","1.","1.2.","1.2.3.4","1.256.0","256.0","1.0-beta","-1.2","1.2+build"};
    for(unsigned i=0;i<sizeof(bad_versions)/sizeof(*bad_versions);i++)assert(store_version_compare(bad_versions[i],"1.0.0",&order)<0);
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
