/* SPDX-License-Identifier: GPL-3.0-or-later
 * Offline rendering through the original OpenTyrian LDS/OPL synthesizer. */
#include "lds_play.h"
#include "opl.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
int audioSampleRate=22050;
static unsigned u32(FILE *f){unsigned n=0;for(int i=0;i<4;i++)n|=(unsigned)fgetc(f)<<(i*8);return n;}
static void put16(FILE *f,unsigned n){fputc(n,f);fputc(n>>8,f);}
static void put32(FILE *f,unsigned n){put16(f,n);put16(f,n>>16);}
int main(int argc,char **argv){
 if(argc!=4)return 2;int song=atoi(argv[2]);FILE *f=fopen(argv[1],"rb");if(!f)return 3;
 unsigned count=fgetc(f);count|=(unsigned)fgetc(f)<<8;if(song<0||song>=count)return 4;
 fseek(f,2+4*song,SEEK_SET);unsigned start=u32(f),end;
 if(song+1<(int)count)end=u32(f);else{fseek(f,0,SEEK_END);end=ftell(f);}
 if(end<=start)return 5;unsigned size=end-start;void *data=malloc(size);fseek(f,start,SEEK_SET);if(fread(data,1,size,f)!=size)return 6;fclose(f);
 opl_init();if(!lds_load(data,size))return 7;free(data);
 f=fopen(argv[3],"wb");if(!f)return 8;fwrite("RIFF",1,4,f);put32(f,0);fwrite("WAVEfmt ",1,8,f);put32(f,16);put16(f,1);put16(f,1);put32(f,audioSampleRate);put32(f,audioSampleRate*2);put16(f,2);put16(f,16);fwrite("data",1,4,f);put32(f,0);
 unsigned frames=0,frac=0;int ticks=0;int16_t pcm[512];
 do{lds_update();frac+=audioSampleRate*2;unsigned n=frac/139;frac%=139;opl_update(pcm,n);fwrite(pcm,2,n,f);frames+=n;ticks++;}while(playing&&!songlooped&&ticks<139*600/2);
 fseek(f,4,SEEK_SET);put32(f,36+frames*2);fseek(f,40,SEEK_SET);put32(f,frames*2);fclose(f);lds_free();
 printf("%d %.3f %d\n",song,(double)frames/audioSampleRate,songlooped);return ticks>=139*600/2?9:0;
}

