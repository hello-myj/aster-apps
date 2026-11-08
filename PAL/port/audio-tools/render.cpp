#include "pal_libc.h"
#include "adplug/rix.h"
#include "embedded/pal_mame_opl2_static.h"
class OfflineOpl final:public Copl {
public:
 OfflineOpl():Copl(TYPE_OPL2){}
 void init()override{PalMameOpl2_Reset();}
 void write(int r,int v)override{PalMameOpl2_Write(r,v);}
 void update(short *p,int n)override{PalMameOpl2_Render(p,n);}
 bool getstereo()override{return false;}
};
static OfflineOpl opl;
static CrixPlayer decoder(&opl);
static uint8_t input[16384];
static int16_t output[256];
static unsigned phase;
extern "C" {
unsigned pal_rix_input(void){return (unsigned)(uintptr_t)input;}
unsigned pal_rix_output(void){return (unsigned)(uintptr_t)output;}
int pal_rix_load(unsigned size){phase=0;return size<=sizeof(input)&&decoder.load_buffer(input,size);}
int pal_rix_tick(void){
 if(!decoder.update())return 0;
 unsigned count=16384/70;phase+=16384%70;
 if(phase>=70){phase-=70;count++;}
 PalMameOpl2_Render(output,count);return (int)count;
}
void __cxa_pure_virtual(void){__builtin_trap();}
}
