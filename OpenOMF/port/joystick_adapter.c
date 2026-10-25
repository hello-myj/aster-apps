/* Preserve upstream hardware handling; append Aster touch as a menu gamepad.
 * This enables upstream character-wheel text entry on the physical board. */
#define joystick_menu_poll_all upstream_joystick_menu_poll_all
#include "../vendor/upstream/src/controller/joystick.c"
#undef joystick_menu_poll_all
int omf_menu_touch_bits(void);
void joystick_menu_poll_all(controller *ctrl,ctrl_event **events){
    upstream_joystick_menu_poll_all(ctrl,events);
    int b=omf_menu_touch_bits();const int action[]={ACT_LEFT,ACT_RIGHT,ACT_UP,ACT_DOWN,ACT_PUNCH,ACT_KICK,ACT_ESC};
    for(int i=0;i<7;i++)if(b&(1<<i))controller_cmd(ctrl,action[i],events);
}
