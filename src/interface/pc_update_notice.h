#ifndef PC_UPDATE_NOTICE_H
#define PC_UPDATE_NOTICE_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* Only published version tokens cross this boundary, never arbitrary UI text. */
static inline int pc_update_version_token(const char *version)
{
    if(*version=='v')version++;
    if(!isdigit((unsigned char)*version))return 0;
    for(size_t i=0;version[i];i++) {
        unsigned char c=(unsigned char)version[i];
        if(i>=95 || !((c>='0'&&c<='9') || (c>='A'&&c<='Z') || (c>='a'&&c<='z') || c=='.' || c=='-' || c=='+'))return 0;
    }
    return 1;
}
static inline int pc_update_notice_available(const char *text,size_t bytes,const char *installed,long long now)
{
    char magic[32],current[96],available[96],extra;long long checked;
    if(!text || bytes>=512 || memchr(text,0,bytes) || !installed || !pc_update_version_token(installed))return 0;
    char copy[512];memcpy(copy,text,bytes);copy[bytes]=0;
    if(sscanf(copy,"%31s %lld %95s %95s %c",magic,&checked,current,available,&extra)!=4 ||
       strcmp(magic,"TSFP_GAME_UPDATE_V1") || checked<=0 || now<=0 ||
       (checked>now && checked-now>300) || (now>checked && now-checked>86400) || !pc_update_version_token(current) || !pc_update_version_token(available))return 0;
    if(*current=='v')memmove(current,current+1,strlen(current));
    if(*installed=='v')installed++;
    return !strcmp(current,installed) && strcmp(current,*available=='v'?available+1:available)!=0;
}
#endif
