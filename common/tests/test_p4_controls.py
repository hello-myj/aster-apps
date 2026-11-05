"""Exercise the real SDL touch adapters: ownership, multitouch and releases."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT.parent/'aster-sdk'


class P4Controls(unittest.TestCase):
    def test_tyrian_menu_translations_font_and_selection_colors(self):
        self.run_adapter('OpenTyrian','port/localization.c',
                         '#include "localization.h"\n',r'''
int main(void){
 assert(tyrian_zh_width("Start New Game",1)==40);
 unsigned char pixels[320*200]={0};SDL_Surface s={0};s.w=320;s.h=200;s.pitch=320;s.pixels=pixels;
 assert(tyrian_zh_draw(&s,20,30,"Start New Game",1,15,0,0));
 int painted=0;for(int y=0;y<200;y++)for(int x=0;x<320;x++)if(pixels[y*320+x]){painted++;assert(y>=30&&y<40);}
 assert(painted);
 memset(pixels,0,sizeof(pixels));
 assert(tyrian_zh_draw(&s,0,0,"Player 1",1,15,0,1));
 for(int i=0;i<320*200;i++)assert(!pixels[i]); /* no old low-res shadow */
 assert(tyrian_zh_draw(&s,0,0,"Player 1",1,15,0,0));
 int low=20,high=-1;for(int y=0;y<12;y++)for(int x=24;x<36;x++)if(pixels[y*320+x]){if(y<low)low=y;if(y>high)high=y;}
 assert(low>=3&&high<10); /* 一 stays around the common baseline, not the top */
 unsigned char a[320*200]={0},b[320*200]={0};s.pixels=a;
 assert(tyrian_zh_draw(&s,20,30,"Upgrade Ship",2,15,3,0));s.pixels=b;
 assert(tyrian_zh_draw(&s,20,30,"Upgrade Ship",2,15,6,0));
 int brighter=0;for(int i=0;i<320*200;i++)if(a[i]){assert(b[i]>a[i]);brighter++;}assert(brighter);
 assert(tyrian_zh_width("Ship Type",2)>0);assert(tyrian_zh_width("Quit Level",2)>0);
 assert(tyrian_zh_width("Reset to Defaults",2)>0);return 0;
}
''')

    def test_native_chinese_copy_fade_and_overwrite(self):
        self.run_adapter('OpenTyrian','port/localization.c',
                         '#include "localization.h"\n#include <string.h>\n',r'''
int main(void){
 unsigned char pixels[320*200],copy[320*200];memset(pixels,17,sizeof(pixels));
 SDL_Surface s={0};s.w=320;s.h=200;s.pitch=320;s.pixels=pixels;
 assert(tyrian_zh_draw(&s,20,30,"Start New Game",1,15,0,0));memcpy(copy,pixels,sizeof(copy));
 uint16_t out[480*480],palette[256];for(int i=0;i<256;i++)palette[i]=0;palette[17]=0x0842;palette[252]=0xffff;
 for(int i=0;i<480*480;i++)out[i]=0x0842;
 tyrian_zh_present(out,480,copy,palette);int ink=0,aa=0;
 for(int y=0;y<480;y++)for(int x=0;x<480;x++)if(out[y*480+x]!=0x0842){assert(x>=30&&x<90&&y>=85&&y<100);ink++;if(out[y*480+x]!=0xffff)aa++;}
 assert(ink>50&&aa>10);
 /* A copied VGA background retains native glyphs; palette fades also apply. */
 palette[17]=palette[252]=0;tyrian_zh_present(out,480,copy,palette);
 for(int y=85;y<100;y++)for(int x=30;x<90;x++)assert(!out[y*480+x]);
 /* Engine overwrite invalidates old text; it must not reappear next frame. */
 memset(copy,17,sizeof(copy));for(int i=0;i<480*480;i++)out[i]=0x1234;
 tyrian_zh_present(out,480,copy,palette);for(int i=0;i<480*480;i++)assert(out[i]==0x1234);
 return 0;
}
''')

    def run_adapter(self, project, source, preamble, body):
        app = ROOT/project
        deps = app/('artifacts/engine-deps' if project == 'OpenOMF' else 'artifacts/deps')
        with tempfile.TemporaryDirectory() as directory:
            c = Path(directory)/'test.c'
            c.write_text('#include <assert.h>\n#include <SDL.h>\n#include <aster_imports.h>\n'+preamble+body,encoding='utf-8')
            out = Path(directory)/'test'
            cmd = ['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wno-attributes',
                   '-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
            cmd += ['-I'+str(p) for p in [app/'port/include',deps/'SDL2/include',
                     app/'vendor/upstream/src',SDK/'include',SDK/'lib/aster']]
            subprocess.run(cmd+[str(c),str(app/source),'-o',str(out)],check=True,capture_output=True,text=True)
            subprocess.run([str(out)],check=True,timeout=10)

    def test_fighting_stick_and_independent_attack_fingers(self):
        self.run_adapter('OpenOMF','port/sdl_adapter.c',r'''
#include "game/utils/settings.h"
static settings config={.keys={.key1_walk_back="Left",.key1_walk_right="Right",.key1_jump_up="Up",.key1_duck="Down",.key1_punch="Return",.key1_kick="Right Shift"}};
settings *settings_get(void){return &config;}
void omf_input_event(const aster_event_t *);void omf_input_mode(int);void omf_input_cancel(void);int omf_menu_touch_bits(void);
static void touch(int slot,int action,int x,int y){aster_event_t e={.type=ASTER_EVENT_TOUCH};e.u.touch.slot=slot;e.u.touch.action=action;e.u.touch.x=x;e.u.touch.y=y;omf_input_event(&e);}
''',r'''
int main(void){
 omf_input_mode(1);const Uint8 *s=SDL_GetKeyboardState(NULL);
 touch(0,ASTER_TOUCH_DOWN,84,414);assert(!s[SDL_SCANCODE_UP]);
 touch(0,ASTER_TOUCH_MOVE,30,380);assert(s[SDL_SCANCODE_UP]&&s[SDL_SCANCODE_LEFT]);
 touch(1,ASTER_TOUCH_DOWN,326,428);assert(s[SDL_SCANCODE_RETURN]&&s[SDL_SCANCODE_LEFT]);
 touch(2,ASTER_TOUCH_DOWN,420,414);assert(s[SDL_SCANCODE_RSHIFT]);
 touch(1,ASTER_TOUCH_UP,326,428);assert(!s[SDL_SCANCODE_RETURN]&&s[SDL_SCANCODE_RSHIFT]&&s[SDL_SCANCODE_LEFT]);
 touch(2,ASTER_TOUCH_UP,420,414);touch(0,ASTER_TOUCH_MOVE,420,414);assert(s[SDL_SCANCODE_RIGHT]&&!s[SDL_SCANCODE_RSHIFT]);
 touch(0,ASTER_TOUCH_UP,420,414);assert(!s[SDL_SCANCODE_RIGHT]);
 touch(0,ASTER_TOUCH_DOWN,326,200);touch(0,ASTER_TOUCH_MOVE,326,428);assert(!s[SDL_SCANCODE_RETURN]);
 omf_input_cancel();omf_input_mode(0);touch(0,ASTER_TOUCH_DOWN,400,20);assert(omf_menu_touch_bits()==64);
 omf_input_cancel();assert(!omf_menu_touch_bits());
 /* MELEE and VS must use the configured player keys, not menu-only events. */
 config.keys.key1_punch="X";config.keys.key1_walk_right="D";
 omf_input_mode(2);touch(0,ASTER_TOUCH_DOWN,326,428);
 assert(s[SDL_SCANCODE_X]&&!s[SDL_SCANCODE_RETURN]&&!omf_menu_touch_bits());
 touch(0,ASTER_TOUCH_UP,326,428);assert(!s[SDL_SCANCODE_X]);
 touch(0,ASTER_TOUCH_DOWN,84,414);touch(0,ASTER_TOUCH_MOVE,124,414);assert(s[SDL_SCANCODE_D]);
 omf_input_mode(1);assert(!s[SDL_SCANCODE_D]);
 omf_input_mode(0);touch(0,ASTER_TOUCH_DOWN,326,428);assert(omf_menu_touch_bits()==16&&!s[SDL_SCANCODE_X]);
 omf_input_cancel();return 0;
}
''')

    def test_shooting_stick_fire_and_sidekicks(self):
        self.run_adapter('OpenTyrian','port/sdl.c',r'''
void tyrian_input(const aster_event_t *);void tyrian_input_clear(void);
uint32_t tyrian_ticks(void){return 0;}void tyrian_video_invalidate(void){}
static void touch(int slot,int action,int x,int y){aster_event_t e={.type=ASTER_EVENT_TOUCH};e.u.touch.slot=slot;e.u.touch.action=action;e.u.touch.x=x;e.u.touch.y=y;tyrian_input(&e);}
''',r'''
int main(void){
 const Uint8 *s=SDL_GetKeyboardState(NULL);
 touch(0,ASTER_TOUCH_DOWN,84,414);touch(0,ASTER_TOUCH_MOVE,30,380);assert(s[SDL_SCANCODE_LEFT]&&s[SDL_SCANCODE_UP]);
 touch(1,ASTER_TOUCH_DOWN,404,422);assert(s[SDL_SCANCODE_SPACE]);
 touch(2,ASTER_TOUCH_DOWN,224,438);assert(s[SDL_SCANCODE_LCTRL]);
 touch(1,ASTER_TOUCH_UP,404,422);assert(!s[SDL_SCANCODE_SPACE]&&s[SDL_SCANCODE_LCTRL]&&s[SDL_SCANCODE_LEFT]);
 touch(2,ASTER_TOUCH_UP,224,438);touch(0,ASTER_TOUCH_UP,30,380);assert(!s[SDL_SCANCODE_UP]);
 touch(0,ASTER_TOUCH_DOWN,404,422);touch(0,ASTER_TOUCH_MOVE,240,370);assert(!s[SDL_SCANCODE_RETURN]);
 tyrian_input_clear();touch(0,ASTER_TOUCH_DOWN,240,370);assert(s[SDL_SCANCODE_RETURN]);
 tyrian_input_clear();for(int i=0;i<SDL_NUM_SCANCODES;i++)assert(!s[i]);return 0;
}
''')


if __name__ == '__main__':
    unittest.main()
