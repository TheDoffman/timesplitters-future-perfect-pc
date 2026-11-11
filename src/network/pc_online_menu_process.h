/* Private to the three platform process adapters. */
#include "pc_online_menu.h"
static int online_menu_launching,online_menu_started;
static PcOnlineMenuState online_menu_state;
static uint32_t online_menu_sequence;
static void online_menu_clear(void)
{
    online_menu_started=0;online_menu_sequence=0;
    memset(&online_menu_state,0,sizeof online_menu_state);
}
static void online_menu_files_clear(const char *ready)
{
    const char *suffix[]={".request",".request.tmp",".state",".state.tmp",".server"};
    char path[4096];
    for(unsigned i=0;i<sizeof suffix/sizeof suffix[0];i++) {
        int size=snprintf(path,sizeof path,"%s%s",ready,suffix[i]);
        if(size>0 && size<(int)sizeof path)remove(path);
    }
}
