#ifndef TSFP_PC_GAME_LANGUAGE_GUEST_H
#define TSFP_PC_GAME_LANGUAGE_GUEST_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Original US guest language indices, not Xbox EEPROM IDs. */
static inline int pc_game_language_selected(void)
{
    const char *code=getenv("TSFP_GAME_LANGUAGE");
    if(!code)return -1;
    if(!strcmp(code,"en"))return 0;
    if(!strcmp(code,"fr"))return 1;
    if(!strcmp(code,"de"))return 2;
    if(!strcmp(code,"it"))return 3;
    if(!strcmp(code,"es"))return 4;
    return -1;
}
/* The US title bypasses XGetLanguage when its region flag is nonzero.
 * Allow only this initialization read to enter the original language path;
 * do not change the region field or its other consumers. */
static inline uint32_t pc_game_language_gate(uint32_t site,uint32_t address,uint32_t value)
{
    if(site==0x70360u && address==0x74CF60u && pc_game_language_selected()>=0)return 0;
    return value;
}
/* The alternate startup path resets English just before reloading text.
 * Keep the selected complete language at that one store as well. */
static inline uint32_t pc_game_language_store(uint32_t site,uint32_t address,uint32_t value)
{
    if(site==0x3C308u && address==0x4C0454u) {
        int selected=pc_game_language_selected();
        if(selected>=0)return (uint32_t)selected;
    }
    return value;
}
#endif
