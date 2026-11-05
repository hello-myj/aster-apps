"""Exercise coroutine stack placement against real WAMR AOT bounds checks.

Linux host prerequisite: firmware build/host/libaster_vm.a, SDK wamrc/Binaryen.
This is a focused stack regression; game/device tests remain separate.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT.parents[1] / 'aster-sdk'
FW = ROOT.parents[1] / 'aster-firmware'


@unittest.skipUnless(os.name == 'posix' and (FW/'build/host/libaster_vm.a').is_file()
                     and (SDK/'toolchain/wamrc').is_file()
                     and (ROOT/'artifacts/deps/binaryen-version_133/bin/wasm-as').is_file(),
                     'requires the Linux WAMR host library, wamrc and Binaryen')
class AuxiliaryStack(unittest.TestCase):
    def test_bss_stack_traps_but_reserved_auxiliary_region_succeeds(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base/'runner.c'
            source.write_text(r'''
#include "wasm_export.h"
#include "aux_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;
 fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);uint8_t *b=malloc(n);
 if(!b||fread(b,1,n,f)!=(size_t)n)return 4;fclose(f);
 if(!wasm_runtime_init())return 5;
 char error[256]={0};wasm_module_t m=wasm_runtime_load(b,n,error,sizeof(error));
 wasm_module_inst_t i=m?wasm_runtime_instantiate(m,65536,0,error,sizeof(error)):NULL;
 if(!i){fprintf(stderr,"%s\n",error);return 6;}
 wasm_exec_env_t env=wasm_runtime_create_exec_env(i,65536);
 if(atoi(argv[2])!=2 && !aster_idf_bind_main_aux_stack(env))return 8;
 wasm_function_inst_t fn=wasm_runtime_lookup_function(i,"probe");uint32_t args[1]={0};
 bool ok=env&&fn&&wasm_runtime_call_wasm(env,fn,0,args);
 const char *exception=wasm_runtime_get_exception(i);
 int pass=atoi(argv[2])==1?ok:(!ok&&exception&&strstr(exception,"auxiliary stack overflow"));
 if(!pass)fprintf(stderr,"ok=%d exception=%s\n",ok,exception?exception:"none");
 if(env)wasm_runtime_destroy_exec_env(env);wasm_runtime_deinstantiate(i);
 wasm_runtime_unload(m);wasm_runtime_destroy();free(b);return pass?0:7;
}
''', encoding='utf-8')
            binary = base/'runner'
            subprocess.run(['cc','-std=c11','-DWASM_ENABLE_THREAD_MGR=1',
                '-DWASM_ENABLE_SHARED_MEMORY=1',
                '-I'+str(FW/'modules/wamr/core/iwasm/include'),str(source),
                '-I'+str(FW/'systems/espidf/components/aster_runtime'),
                str(FW/'build/host/libaster_vm.a'),'-lm','-ldl','-lpthread','-o',str(binary)],check=True)
            for name, stack, expected in [('bss',32768,'0'),('partitioned',327680,'2'),('aux',327680,'1')]:
                wat=base/(name+'.wat');wasm=base/(name+'.wasm');aot=base/(name+'.aot')
                wat.write_text(f'''(module
 (memory (export "memory") 8)
 (global $sp (mut i32) (i32.const 393216))
 (global $end i32 (i32.const 65536))
 (global $heap i32 (i32.const 393216))
 (export "__stack_pointer" (global $sp))
 (export "__data_end" (global $end))
 (export "__heap_base" (global $heap))
 (func (export "probe")
   (global.set $sp (i32.const {stack}))
   (global.set $sp (i32.const 393216))))''',encoding='utf-8')
                subprocess.run([str(ROOT/'artifacts/deps/binaryen-version_133/bin/wasm-as'),str(wat),'-o',str(wasm)],check=True)
                subprocess.run([os.environ.get('ASTER_WAMRC',str(SDK/'toolchain/wamrc')),
                    '--target=x86_64','--bounds-checks=1','--stack-bounds-checks=1',
                    '-o',str(aot),str(wasm)],check=True,capture_output=True)
                subprocess.run([str(binary),str(aot),expected],check=True,timeout=10)


if __name__ == '__main__':
    unittest.main()
