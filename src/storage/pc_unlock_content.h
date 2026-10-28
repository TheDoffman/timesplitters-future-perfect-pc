#ifndef TSFP_PC_UNLOCK_CONTENT_H
#define TSFP_PC_UNLOCK_CONTENT_H
#include <stdint.h>
#include <string.h>

enum PcUnlockCategory {
    PC_UNLOCK_STORY, PC_UNLOCK_CHARACTERS, PC_UNLOCK_MAPS, PC_UNLOCK_MODES,
    PC_UNLOCK_WEAPONS, PC_UNLOCK_LEAGUE, PC_UNLOCK_CHALLENGES,
    PC_UNLOCK_CHEATS, PC_UNLOCK_MOVIES, PC_UNLOCK_COUNT
};
#define PC_UNLOCK_ALL ((1u << PC_UNLOCK_COUNT)-1u)
#define PC_UNLOCK_PROFILE_SIZE 0x1f54u
/* US v2 saves end their authored fields with version at 1EE0. Use only
 * sixteen reserved bytes, inside the existing signed/copied profile body.
 * Unknown/nonzero extension data is preserved, never adopted or overwritten. */
#define PC_UNLOCK_OFFSET 0x1ee4u
#define PC_UNLOCK_MAGIC 0x43555354u /* TSUC, little endian */
static int pc_unlock_decode(const void *profile, uint32_t *flags)
{
    uint32_t words[4];
    *flags=0;
    memcpy(words,(const unsigned char *)profile+PC_UNLOCK_OFFSET,sizeof words);
    if(!(words[0]|words[1]|words[2]|words[3]))return 1;
    if(words[0]!=PC_UNLOCK_MAGIC || words[1]!=1 ||
       (words[2]&~PC_UNLOCK_ALL) || words[3]!=~words[2])return 0;
    *flags=words[2];return 1;
}
static int pc_unlock_set(void *profile,unsigned category,int enabled)
{
    uint32_t flags,words[4];
    if(category>=PC_UNLOCK_COUNT || !pc_unlock_decode(profile,&flags))return 0;
    if(enabled)flags|=1u<<category;else flags&=~(1u<<category);
    words[0]=PC_UNLOCK_MAGIC;words[1]=1;words[2]=flags;words[3]=~flags;
    memcpy((unsigned char *)profile+PC_UNLOCK_OFFSET,words,sizeof words);
    return 1;
}
/* Only known access predicates can consume overrides. Completion/award checks
 * and collectible pickup tests must continue reading the original progress. */
static int pc_unlock_requirement_category(uint32_t caller,uint32_t descriptor)
{
    switch(caller) {
    case 0x1cd8da: return PC_UNLOCK_STORY;
    case 0x1cd9bb: return PC_UNLOCK_CHARACTERS;
    case 0x1cda18: return PC_UNLOCK_MAPS;
    case 0x1cd916: case 0x1cd8fc: return PC_UNLOCK_MODES;
    case 0x1cd9e4: return PC_UNLOCK_WEAPONS;
    case 0x1cd974: case 0x1cd936: return PC_UNLOCK_CHEATS;
    case 0x1cda5b:
        if(descriptor>=0x510804u && descriptor<0x510804u+7u*0x38u &&
           (descriptor-0x510804u)%0x38u==0)return PC_UNLOCK_CHALLENGES;
        if(descriptor>=0x513b84u && descriptor<0x513b84u+9u*0x38u &&
           (descriptor-0x513b84u)%0x38u==0)return PC_UNLOCK_LEAGUE;
        break;
    }
    return -1;
}
#endif
