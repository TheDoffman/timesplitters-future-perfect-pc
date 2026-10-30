#ifndef TSFP_PC_KEYBOARD_MENU_MODEL_H
#define TSFP_PC_KEYBOARD_MENU_MODEL_H
#include <math.h>
/* Keep every action reachable once, including both primary and alternate bindings. */
static const char *const pc_keyboard_action_labels[]={
    "Interact / confirm","Melee / back","Reload","Y action","Black action","White action",
    "Pause","Objectives","Throw grenade","Fire","Mode / zoom in","Grenade / zoom out",
    "Previous weapon","Next weapon","Forward","Backward","Strafe left","Strafe right",
    "Crouch","Look up","Look down","Look left","Look right","Aim",
#define PC_WEAPON_SLOT(n,key) "Weapon slot " #n,
#include "pc_weapon_slots.def"
#undef PC_WEAPON_SLOT
};
static const int pc_keyboard_groups[4][12]={
    {14,15,16,17,18,19,20,21,22,-1,-1,-1},
    {1,2,8,9,10,11,23,-1,-1,-1,-1,-1},
    {12,13,24,25,26,27,28,29,30,31,32,33},
    {0,3,4,5,6,7,-1,-1,-1,-1,-1,-1}
};
static const char *const pc_keyboard_group_names[]={"Movement and view","Combat","Weapons","Menu actions"};
static inline double pc_keyboard_sensitivity(unsigned step)
{ if(step>1000)step=1000;return .001*pow(10000.,(double)step/1000.); }
static inline unsigned pc_keyboard_sensitivity_step(double value)
{ if(!isfinite(value)||value<=.001)return 0;if(value>=10)return 1000;return (unsigned)(log(value/.001)/log(10000.)*1000.+.5); }
#endif
