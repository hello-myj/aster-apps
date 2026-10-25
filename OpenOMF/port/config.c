/* Flat configuration backend for the upstream settings registry. */
#include "utils/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef struct {const char *name;int type;double n;char *s;} Opt;
static Opt opts[160];static int count;
static Opt *find(const char *key){for(int i=0;i<count;i++)if(!strcmp(opts[i].name,key))return &opts[i];return NULL;}
static Opt *add(char *name,int t){Opt *o=find(name);if(o)return o;if(count==160)abort();o=&opts[count++];o->name=name;o->type=t;return o;}
void conf_addint(char *n,int v){add(n,0)->n=v;}
void conf_addbool(char *n,int v){add(n,1)->n=v;}
void conf_addfloat(char *n,double v){add(n,2)->n=v;}
void conf_addstring(char *n,char *v){Opt *o=add(n,3);free(o->s);o->s=v?strdup(v):NULL;}
int conf_int(const char *n){Opt *o=find(n);return o?(int)o->n:0;}
int conf_bool(const char *n){return conf_int(n)!=0;}
double conf_float(const char *n){Opt *o=find(n);return o?o->n:0;}
const char *conf_string(const char *n){Opt *o=find(n);return o?o->s:NULL;}
void conf_setint(const char *n,int v){Opt *o=find(n);if(o)o->n=v;}
void conf_setbool(const char *n,int v){conf_setint(n,v);}
void conf_setfloat(const char *n,double v){Opt *o=find(n);if(o)o->n=v;}
void conf_setstring(const char *n,const char *v){Opt *o=find(n);if(o){char *s=v?strdup(v):NULL;free(o->s);o->s=s;}}
int conf_write_config(const char *name){FILE *f=fopen(name,"w");if(!f)return 1;for(int i=0;i<count;i++){Opt *o=&opts[i];fprintf(f,"%s=",o->name);if(o->type==3){fputc('"',f);for(const char *s=o->s?o->s:"";*s;s++){if(*s=='"'||*s=='\\')fputc('\\',f);fputc(*s,f);}fputc('"',f);}else if(o->type==1)fputs(o->n?"true":"false",f);else fprintf(f,"%.17g",o->n);fputc('\n',f);}int e=ferror(f);if(fclose(f))e=1;return e!=0;}
int conf_init(const char *name){FILE *f=fopen(name,"r");if(!f)return conf_write_config(name);char line[2048];while(fgets(line,sizeof(line),f)){char *eq=strchr(line,'=');if(!eq)continue;*eq=0;char *key=line;while(isspace((unsigned char)*key))key++;char *end=eq;while(end>key&&isspace((unsigned char)end[-1]))*--end=0;Opt *o=find(key);if(!o)continue;char *v=eq+1;while(isspace((unsigned char)*v))v++;if(o->type==3){char text[2048];int n=0;if(*v=='"')v++;while(*v&&*v!='"'&&*v!='\r'&&*v!='\n'){if(*v=='\\'&&v[1])v++;text[n++]=*v++;}text[n]=0;conf_setstring(key,text);}else if(o->type==1)o->n=!strncmp(v,"true",4)||atoi(v)!=0;else o->n=strtod(v,NULL);}int e=ferror(f);fclose(f);return e!=0;}
void conf_close(void){for(int i=0;i<count;i++)free(opts[i].s);memset(opts,0,sizeof(opts));count=0;}
